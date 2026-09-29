#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main()
{
    int client;
    struct sockaddr_in server_address;
    char buffer[1024];

    // ==========================================
    // STEP 1: CREATE CLIENT SOCKET
    // ==========================================

    client = socket(AF_INET, SOCK_STREAM, 0);

    if (client < 0)
    {
        printf("Client socket creation failed.\n");
        return 1;
    }

    printf("Client socket created successfully!\n");


    // ==========================================
    // STEP 2: SET SERVER ADDRESS
    // ==========================================

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");


    // ==========================================
    // STEP 3: CONNECT TO SERVER
    // ==========================================

    if (connect(
            client,
            (struct sockaddr *)&server_address,
            sizeof(server_address)) < 0)
    {
        printf("Connection to server failed.\n");
        close(client);
        return 1;
    }

    printf("Connected to server successfully!\n");


    // ==========================================
    // STEP 4: RECEIVE SERVER WELCOME MESSAGE
    // ==========================================

    int bytes_received;

    bytes_received = recv(
        client,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf(
            "Server message: %s\n",
            buffer
        );
    }


    // ==========================================
    // STEP 5: SEND MESSAGE TO SERVER
    // ==========================================

    const char *message = "Hello Server!";

    if (send(
            client,
            message,
            strlen(message),
            0) < 0)
    {
        printf("Failed to send message.\n");
        close(client);
        return 1;
    }

    printf("Message sent to server.\n");


    // ==========================================
    // STEP 6: RECEIVE SERVER RESPONSE
    // ==========================================

    bytes_received = recv(
        client,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf(
            "Server response: %s\n",
            buffer
        );
    }
    else if (bytes_received == 0)
    {
        printf("Server disconnected.\n");
    }
    else
    {
        printf("Error receiving server response.\n");
    }


    // ==========================================
    // STEP 7: KEEP CLIENT CONNECTED
    // ==========================================

    printf("Client is staying connected...\n");
    printf("Press ENTER to disconnect.\n");

    getchar();


    // ==========================================
    // STEP 8: DISCONNECT AND CLEANUP
    // ==========================================

    close(client);

    printf("Client disconnected successfully.\n");

    return 0;
}