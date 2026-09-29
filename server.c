#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CLIENTS 3

int main()
{
    int server;
    int clients[MAX_CLIENTS];
    struct sockaddr_in server_address;
    fd_set readfds;
    char buffer[1024];

    // ==========================================
    // STEP 1: CREATE SERVER SOCKET
    // ==========================================

    server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0)
    {
        printf("Socket creation failed.\n");
        return 1;
    }

    printf("Server socket created successfully!\n");


    // ==========================================
    // STEP 2: BIND SERVER TO IP ADDRESS AND PORT
    // ==========================================

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(8080);

    if (bind(server, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        printf("Bind failed.\n");
        close(server);
        return 1;
    }

    printf("Server bound to 127.0.0.1:8080\n");


    // ==========================================
    // STEP 3: LISTEN FOR CLIENT CONNECTIONS
    // ==========================================

    if (listen(server, 3) < 0)
    {
        printf("Listen failed.\n");
        close(server);
        return 1;
    }

    printf("Server is listening for clients...\n");


    // ==========================================
    // STEP 4: PREPARE CLIENT CONNECTIONS
    // ==========================================

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i] = -1;
    }

    printf("Server is ready to accept up to %d clients.\n",
           MAX_CLIENTS);


    // ==========================================
    // STEP 5: ACCEPT CLIENT CONNECTIONS
    // ==========================================

    while (1)
    {
        FD_ZERO(&readfds);

        FD_SET(server, &readfds);

        // Add connected clients to the set
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != -1)
            {
                FD_SET(clients[i], &readfds);
            }
        }


        // ==========================================
        // STEP 6: HANDLE MULTIPLE CLIENTS
        // ==========================================

        int max_fd = server;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] > max_fd)
            {
                max_fd = clients[i];
            }
        }

        int activity = select(
            max_fd + 1,
            &readfds,
            NULL,
            NULL,
            NULL
        );

        if (activity < 0)
        {
            printf("Select failed.\n");
            break;
        }


        // ==========================================
        // STEP 7: HANDLE NEW / RECONNECTED CLIENT
        // ==========================================

        if (FD_ISSET(server, &readfds))
        {
            int new_client;

            new_client = accept(server, NULL, NULL);

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

                        printf(
                            "Client %d connected/reconnected successfully!\n",
                            i + 1
                        );

                        const char *welcome =
                            "Connected to server successfully!";

                        send(
                            new_client,
                            welcome,
                            strlen(welcome),
                            0
                        );

                        break;
                    }
                }

                if (!added)
                {
                    printf(
                        "Maximum number of clients reached.\n"
                    );

                    close(new_client);
                }
            }
        }


        // ==========================================
        // STEP 8: RECEIVE DATA AND SEND SHARED FILE
        // ==========================================

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != -1 &&
                FD_ISSET(clients[i], &readfds))
            {
                int bytes_received;

                bytes_received = recv(
                    clients[i],
                    buffer,
                    sizeof(buffer) - 1,
                    0
                );


                // ------------------------------------------
                // CLIENT DISCONNECTED
                // ------------------------------------------

                if (bytes_received == 0)
                {
                    printf(
                        "Client %d disconnected.\n",
                        i + 1
                    );

                    close(clients[i]);
                    clients[i] = -1;
                }


                // ------------------------------------------
                // RECEIVE ERROR
                // ------------------------------------------

                else if (bytes_received < 0)
{
    printf(
        "Error receiving data from Client %d: %s\n",
        i + 1,
        strerror(errno)
    );

    close(clients[i]);
    clients[i] = -1;
}


                // ------------------------------------------
                // MESSAGE RECEIVED
                // ------------------------------------------

                else
                {
                    buffer[bytes_received] = '\0';

                    printf(
                        "Message from Client %d: %s\n",
                        i + 1,
                        buffer
                    );

                    const char *response =
                        "Message received by server!";

                    send(
                        clients[i],
                        response,
                        strlen(response),
                        0
                    );

                    printf(
                        "Response sent to Client %d.\n",
                        i + 1
                    );


                    // ------------------------------------------
                    // SEND CURRENT SHARED FILE
                    // ------------------------------------------

                    FILE *file;
                    char file_buffer[4096];
                    size_t bytes_read;

                    file = fopen("shared.txt", "r");

                    if (file == NULL)
                    {
                        printf(
                            "Could not open shared.txt\n"
                        );

                        const char *error_message =
                            "Could not open shared file.";

                        send(
                            clients[i],
                            error_message,
                            strlen(error_message),
                            0
                        );
                    }
                    else
                    {
                        bytes_read = fread(
                            file_buffer,
                            1,
                            sizeof(file_buffer) - 1,
                            file
                        );

                        file_buffer[bytes_read] = '\0';

                        fclose(file);

                        if (bytes_read > 0)
                        {
                            send(
                                clients[i],
                                file_buffer,
                                bytes_read,
                                0
                            );

                            printf(
                                "Current shared file sent to Client %d.\n",
                                i + 1
                            );
                        }
                        else
                        {
                            printf(
                                "Shared file is empty.\n"
                            );
                        }
                    }
                }
            }
        }
    }


    // ==========================================
    // CLEANUP
    // ==========================================

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != -1)
        {
            close(clients[i]);
        }
    }

    close(server);

    return 0;
}
