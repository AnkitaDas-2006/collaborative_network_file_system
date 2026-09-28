#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 3

int main()
{
    WSADATA wsa;
    SOCKET server;
    SOCKET clients[MAX_CLIENTS];
    struct sockaddr_in server_address;
    fd_set readfds;
    char buffer[1024];

    // ==========================================
    // INITIALIZE WINDOWS SOCKET SYSTEM
    // ==========================================

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed.\n");
        return 1;
    }

    // ==========================================
    // STEP 1: CREATE SERVER SOCKET
    // ==========================================

    server = socket(AF_INET, SOCK_STREAM, 0);

    if (server == INVALID_SOCKET)
    {
        printf("Socket creation failed.\n");
        WSACleanup();
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
             sizeof(server_address)) == SOCKET_ERROR)
    {
        printf("Bind failed.\n");
        closesocket(server);
        WSACleanup();
        return 1;
    }

    printf("Server bound to 127.0.0.1:8080\n");

    // ==========================================
    // STEP 3: LISTEN FOR CLIENT CONNECTIONS
    // ==========================================

    if (listen(server, 3) == SOCKET_ERROR)
    {
        printf("Listen failed.\n");
        closesocket(server);
        WSACleanup();
        return 1;
    }

    printf("Server is listening for clients...\n");

    // ==========================================
    // STEP 4: PREPARE CLIENT CONNECTIONS
    // ==========================================

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i] = INVALID_SOCKET;
    }

    // ==========================================
    // STEP 6: HANDLE MULTIPLE CLIENTS
    // ==========================================

    while (1)
    {
        FD_ZERO(&readfds);

        // Add server socket
        FD_SET(server, &readfds);

        // Add connected clients
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != INVALID_SOCKET)
            {
                FD_SET(clients[i], &readfds);
            }
        }

        // Wait for activity
        int activity = select(0, &readfds, NULL, NULL, NULL);

        if (activity == SOCKET_ERROR)
        {
            printf("Select failed.\n");
            break;
        }

        // ==========================================
        // STEP 7: HANDLE NEW / RECONNECTED CLIENT
        // ==========================================

        if (FD_ISSET(server, &readfds))
        {
            SOCKET new_client;

            new_client = accept(server, NULL, NULL);

            if (new_client == INVALID_SOCKET)
            {
                printf("Failed to accept client.\n");
            }
            else
            {
                int added = 0;

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i] == INVALID_SOCKET)
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
                            (int)strlen(welcome),
                            0
                        );
// ==========================================================
// STEP 8: SEND CURRENT SHARED FILE TO CLIENT
// ==========================================================

{
    FILE *file;
    char file_buffer[4096];
    size_t bytes_read;

    file = fopen("shared.txt", "r");

    if (file == NULL)
    {
        printf("Could not open shared.txt\n");

        const char *error_message =
            "Could not open shared file.";

        send(
            new_client,
            error_message,
            (int)strlen(error_message),
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
                new_client,
                file_buffer,
                (int)bytes_read,
                0
            );

            printf(
                "Current shared file sent to Client %d.\n",
                i + 1
            );
        }
        else
        {
            printf("Shared file is empty.\n");
        }
    }
}
                        break;
                    }
                }

                if (!added)
                {
                    printf("Maximum number of clients reached.\n");
                    closesocket(new_client);
                }
            }
        }

        // ==========================================
        // RECEIVE DATA FROM CLIENTS
        // ==========================================

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != INVALID_SOCKET &&
                FD_ISSET(clients[i], &readfds))
            {
                int bytes_received;

                bytes_received = recv(
                    clients[i],
                    buffer,
                    sizeof(buffer) - 1,
                    0
                );

                // ==========================================
                // CLIENT DISCONNECTED
                // ==========================================

                if (bytes_received == 0)
                {
                    printf("Client %d disconnected.\n", i + 1);

                    closesocket(clients[i]);
                    clients[i] = INVALID_SOCKET;
                }

                // ==========================================
                // RECEIVE ERROR
                // ==========================================

                else if (bytes_received == SOCKET_ERROR)
                {
                    printf(
                        "Error receiving data from Client %d.\n",
                        i + 1
                    );

                    closesocket(clients[i]);
                    clients[i] = INVALID_SOCKET;
                }

                // ==========================================
                // MESSAGE RECEIVED
                // ==========================================

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
                        (int)strlen(response),
                        0
                    );

                    printf(
                        "Response sent to Client %d.\n",
                        i + 1
                    );
                }
            }
        }
    }

    // ==========================================
    // CLEANUP
    // ==========================================

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != INVALID_SOCKET)
        {
            closesocket(clients[i]);
        }
    }

    closesocket(server);
    WSACleanup();

    return 0;
}
