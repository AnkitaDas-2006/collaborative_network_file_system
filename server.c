#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CLIENTS 3
#define MAX_FILE_SIZE 4096

int send_all(int socket, const char *data, int length)
{
    int total = 0;

    while (total < length)
    {
        int sent = send(socket, data + total, length - total, 0);

        if (sent <= 0)
            return -1;

        total += sent;
    }

    return total;
}

int recv_line(int socket, char *buffer, int size)
{
    int index = 0;
    char ch;

    while (index < size - 1)
    {
        int result = recv(socket, &ch, 1, 0);

        if (result <= 0)
            return result;

        if (ch == '\n')
            break;

        buffer[index++] = ch;
    }

    buffer[index] = '\0';
    return index;
}

int recv_all(int socket, char *buffer, int length)
{
    int total = 0;

    while (total < length)
    {
        int received = recv(socket,
                            buffer + total,
                            length - total,
                            0);

        if (received <= 0)
            return -1;

        total += received;
    }

    return total;
}

int read_shared_file(char *buffer)
{
    FILE *file = fopen("shared.txt", "rb");

    if (file == NULL)
        return 0;

    int size = fread(buffer, 1, MAX_FILE_SIZE, file);

    fclose(file);

    return size;
}

int write_shared_file(const char *buffer, int size)
{
    FILE *file = fopen("shared.txt", "wb");

    if (file == NULL)
        return -1;

    int written = fwrite(buffer, 1, size, file);

    fclose(file);

    return (written == size) ? 0 : -1;
}

void send_current_file(int client, int version)
{
    char file_buffer[MAX_FILE_SIZE];
    char header[100];

    int file_size = read_shared_file(file_buffer);

    sprintf(header, "FILE_DATA %d %d\n", version, file_size);

    send_all(client, header, strlen(header));

    if (file_size > 0)
        send_all(client, file_buffer, file_size);
}

void broadcast_update(int clients[],
                      int sender,
                      int version,
                      const char *file_buffer,
                      int file_size)
{
    char header[100];

    sprintf(header, "FILE_UPDATE %d %d\n", version, file_size);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != -1 && clients[i] != sender)
        {
            send_all(clients[i], header, strlen(header));

            if (file_size > 0)
                send_all(clients[i], file_buffer, file_size);
        }
    }
}

int main()
{
    int server;
    int clients[MAX_CLIENTS];

    struct sockaddr_in server_address;
    fd_set readfds;

    int file_version = 1;

    // STEP 1: CREATE SERVER SOCKET

    server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0)
    {
        printf("Socket creation failed.\n");
        return 1;
    }

    printf("Server socket created successfully!\n");

    int reuse = 1;

    setsockopt(server,
               SOL_SOCKET,
               SO_REUSEADDR,
               &reuse,
               sizeof(reuse));


    // STEP 2: BIND SERVER TO IP ADDRESS AND PORT

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(8080);

    if (bind(server,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        printf("Bind failed: %s\n", strerror(errno));
        close(server);
        return 1;
    }

    printf("Server bound to 127.0.0.1:8080\n");


    // STEP 3: LISTEN FOR CLIENT CONNECTIONS

    if (listen(server, 3) < 0)
    {
        printf("Listen failed.\n");
        close(server);
        return 1;
    }

    printf("Server is listening for clients...\n");


    // STEP 4: PREPARE CLIENT CONNECTIONS

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;

    printf("Server is ready to accept up to %d clients.\n",
           MAX_CLIENTS);


    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);

        int max_fd = server;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != -1)
            {
                FD_SET(clients[i], &readfds);

                if (clients[i] > max_fd)
                    max_fd = clients[i];
            }
        }


        // STEP 5: ACCEPT CLIENT CONNECTIONS

        int activity = select(max_fd + 1,
                              &readfds,
                              NULL,
                              NULL,
                              NULL);

        if (activity < 0)
        {
            printf("Select failed.\n");
            break;
        }

        if (FD_ISSET(server, &readfds))
        {
            int new_client = accept(server, NULL, NULL);

            if (new_client < 0)
            {
                printf("Failed to accept client.\n");
            }
            else
            {
                int added = 0;

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i] == -1)
                    {
                        clients[i] = new_client;
                        added = 1;

                        printf("Client %d connected/reconnected successfully!\n",
                               i + 1);

                        const char *welcome = "WELCOME\n";

                        send_all(new_client,
                                 welcome,
                                 strlen(welcome));

                        break;
                    }
                }

                if (!added)
                {
                    printf("Maximum number of clients reached.\n");
                    close(new_client);
                }
            }
        }


        // STEP 6: HANDLE MULTIPLE CLIENTS

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != -1 &&
                FD_ISSET(clients[i], &readfds))
            {
                char command[100];

                int result = recv_line(clients[i],
                                       command,
                                       sizeof(command));

                if (result == 0)
                {
                    printf("Client %d disconnected.\n", i + 1);

                    close(clients[i]);
                    clients[i] = -1;

                    continue;
                }

                if (result < 0)
                {
                    printf("Error receiving from Client %d: %s\n",
                           i + 1,
                           strerror(errno));

                    close(clients[i]);
                    clients[i] = -1;

                    continue;
                }


                // STEP 7: HANDLE FILE REQUEST AND UPDATE PROTOCOL

                if (strcmp(command, "GET_FILE") == 0)
                {
                    printf("Client %d requested the current file.\n",
                           i + 1);

                    send_current_file(clients[i],
                                      file_version);
                }

                else if (strncmp(command, "UPDATE ", 7) == 0)
                {
                    int client_version;
                    int file_size;

                    if (sscanf(command + 7,
                               "%d %d",
                               &client_version,
                               &file_size) != 2)
                    {
                        const char *error =
                            "ERROR Invalid UPDATE command\n";

                        send_all(clients[i],
                                 error,
                                 strlen(error));

                        continue;
                    }

                    if (file_size < 0 ||
                        file_size > MAX_FILE_SIZE)
                    {
                        const char *error =
                            "ERROR File too large\n";

                        send_all(clients[i],
                                 error,
                                 strlen(error));

                        continue;
                    }

                    char new_file[MAX_FILE_SIZE];

                    if (file_size > 0)
                    {
                        if (recv_all(clients[i],
                                     new_file,
                                     file_size) < 0)
                        {
                            printf("Failed to receive file.\n");

                            close(clients[i]);
                            clients[i] = -1;

                            continue;
                        }
                    }


                    if (client_version != file_version)
                    {
                        char current_file[MAX_FILE_SIZE];
                        char conflict_header[100];

                        int current_size =
                            read_shared_file(current_file);

                        sprintf(conflict_header,
                                "CONFLICT %d %d\n",
                                file_version,
                                current_size);

                        send_all(clients[i],
                                 conflict_header,
                                 strlen(conflict_header));

                        if (current_size > 0)
                        {
                            send_all(clients[i],
                                     current_file,
                                     current_size);
                        }

                        printf("Conflict detected for Client %d.\n",
                               i + 1);
                    }
                    else
                    {
                        file_version++;

                        if (write_shared_file(new_file,
                                              file_size) < 0)
                        {
                            const char *error =
                                "ERROR Could not save file\n";

                            send_all(clients[i],
                                     error,
                                     strlen(error));

                            continue;
                        }

                        char success[100];

                        sprintf(success,
                                "UPDATE_OK %d\n",
                                file_version);

                        send_all(clients[i],
                                 success,
                                 strlen(success));

                        broadcast_update(clients,
                                         clients[i],
                                         file_version,
                                         new_file,
                                         file_size);

                        printf("File updated successfully. Version: %d\n",
                               file_version);
                    }
                }

                else
                {
                    const char *error =
                        "ERROR Unknown command\n";

                    send_all(clients[i],
                             error,
                             strlen(error));
                }
            }
        }


        // STEP 8: SEND UPDATED FILE TO OTHER CLIENTS

        /*
            Accepted updates are sent to all other
            connected clients by broadcast_update().
        */
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != -1)
            close(clients[i]);
    }

    close(server);

    return 0;
}
