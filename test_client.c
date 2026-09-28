#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main()
{
    WSADATA wsa;
    SOCKET client;
    struct sockaddr_in server_address;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup failed.\n");
        return 1;
    }

    client = socket(AF_INET, SOCK_STREAM, 0);

    if (client == INVALID_SOCKET)
    {
        printf("Client socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    printf("Client socket created successfully!\n");

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(8080);

    if (connect(client, (struct sockaddr *)&server_address,
                sizeof(server_address)) == SOCKET_ERROR)
    {
        printf("Connection to server failed.\n");
        closesocket(client);
        WSACleanup();
        return 1;
    }

    printf("Connected to server successfully!\n");

    // ==========================================
    // SEND MESSAGE TO SERVER
    // ==========================================

    const char *message = "Hello Server!";

    send(client, message, (int)strlen(message), 0);

    printf("Message sent to server.\n");

    // ==========================================
    // RECEIVE SERVER RESPONSE
    // ==========================================

    char buffer[1024];

    int bytes_received = recv(client, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Server response: %s\n", buffer);
    }

    // ==========================================
    // KEEP CLIENT CONNECTED
    // ==========================================

    printf("Client is staying connected...\n");
    printf("Press ENTER to disconnect.\n");

    getchar();

    // ==========================================
    // CLEANUP
    // ==========================================

    closesocket(client);
    WSACleanup();

    return 0;
}