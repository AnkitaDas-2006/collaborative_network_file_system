#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

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

int main()
{
    int client;
    struct sockaddr_in server_address;

    char buffer[MAX_FILE_SIZE + 1];

    int file_version = 1;


    // STEP 1: CREATE CLIENT SOCKET

    client = socket(AF_INET, SOCK_STREAM, 0);

    if (client < 0)
    {
        printf("Client socket creation failed.\n");
        return 1;
    }

    printf("Client socket created successfully!\n");


    // STEP 2: SET SERVER ADDRESS

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");


    // STEP 3: CONNECT TO SERVER

    if (connect(client,
                (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0)
    {
        printf("Connection to server failed.\n");
        close(client);
        return 1;
    }

    printf("Connected to server successfully!\n");


    // STEP 4: RECEIVE WELCOME MESSAGE

    if (recv_line(client,
                  buffer,
                  sizeof(buffer)) > 0)
    {
        printf("Server: %s\n", buffer);
    }


    // STEP 5: REQUEST CURRENT SHARED FILE

    const char *request = "GET_FILE\n";

    send_all(client,
             request,
             strlen(request));

    printf("Requested current shared file.\n");


    // RECEIVE FILE_DATA

    if (recv_line(client,
                  buffer,
                  sizeof(buffer)) <= 0)
    {
        printf("Failed to receive file information.\n");
        close(client);
        return 1;
    }

    int received_version;
    int file_size;

    if (sscanf(buffer,
               "FILE_DATA %d %d",
               &received_version,
               &file_size) == 2)
    {
        file_version = received_version;

        printf("File version: %d\n", file_version);
        printf("File size: %d bytes\n", file_size);

        if (file_size > 0 &&
            file_size <= MAX_FILE_SIZE)
        {
            recv_all(client,
                     buffer,
                     file_size);

            buffer[file_size] = '\0';

            printf("\n----- SHARED FILE -----\n");
            printf("%s\n", buffer);
            printf("----- END FILE -----\n");
        }
    }


    // STEP 6: SEND FILE UPDATE

    char choice;

    printf("\nDo you want to update the file? (y/n): ");
    scanf(" %c", &choice);
    getchar();

    if (choice == 'y' || choice == 'Y')
    {
        char new_file[MAX_FILE_SIZE];

        printf("Enter new file content:\n");

        fgets(new_file,
              sizeof(new_file),
              stdin);

        int new_size = strlen(new_file);

        if (new_size > 0 &&
            new_file[new_size - 1] == '\n')
        {
            new_file[new_size - 1] = '\0';
            new_size--;
        }

        char update_header[100];

        sprintf(update_header,
                "UPDATE %d %d\n",
                file_version,
                new_size);

        send_all(client,
                 update_header,
                 strlen(update_header));

        if (new_size > 0)
        {
            send_all(client,
                     new_file,
                     new_size);
        }

        printf("File update sent to server.\n");


        // STEP 7: RECEIVE SERVER RESPONSE

        if (recv_line(client,
                      buffer,
                      sizeof(buffer)) > 0)
        {
            printf("Server: %s\n", buffer);

            int new_version;

            if (sscanf(buffer,
                       "UPDATE_OK %d",
                       &new_version) == 1)
            {
                file_version = new_version;

                printf("File update successful.\n");
                printf("New file version: %d\n",
                       file_version);
            }

            int conflict_version;
            int conflict_size;

            if (sscanf(buffer,
                       "CONFLICT %d %d",
                       &conflict_version,
                       &conflict_size) == 2)
            {
                printf("File update conflict detected.\n");
                printf("Server file version: %d\n",
                       conflict_version);

                if (conflict_size > 0 &&
                    conflict_size <= MAX_FILE_SIZE)
                {
                    recv_all(client,
                             buffer,
                             conflict_size);

                    buffer[conflict_size] = '\0';

                    printf("\n----- LATEST SERVER FILE -----\n");
                    printf("%s\n", buffer);
                    printf("----- END FILE -----\n");
                }
            }
        }
    }


    // STEP 8: DISCONNECT AND CLEANUP

    printf("\nPress ENTER to disconnect.\n");
    getchar();

    close(client);

    printf("Client disconnected successfully.\n");

    return 0;
}