#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#define MAX_FILE_SIZE 4096
int receive_line(int sockfd, char *buffer, int size)
{
    int index = 0;
    char ch;

    while (index < size - 1)
    {
        int bytes = recv(sockfd, &ch, 1, 0);

        if (bytes <= 0)
        {
            return -1;
        }

        if (ch == '\n')
        {
            break;
        }

        buffer[index++] = ch;
    }

    buffer[index] = '\0';

    return index;
}
int receive_file(int sockfd, int *version)
{
    char header[100];
    char file_buffer[MAX_FILE_SIZE];

    if (receive_line(sockfd, header, sizeof(header)) < 0)
    {
       return -1;
    }
    int file_size;

    if (sscanf(header, "FILE_DATA %d %d", version, &file_size) != 2)
    {
        printf("Invalid file data received from server\n");
        return -1;
    }

    if (file_size < 0 || file_size > MAX_FILE_SIZE)
    {
        printf("Invalid file size received from server\n");
        return -1;
    }

    int total = 0;

    while (total < file_size)
    {
        int bytes = recv(sockfd,
                         file_buffer + total,
                         file_size - total,
                         0);

        if (bytes <= 0)
        {
            return -1;
        }

        total += bytes;
    }

    FILE *fp;

    fp = fopen("shared.txt", "wb");

    if (fp == NULL)
    {
        printf("Unable to open shared.txt\n");
        return -1;
    }

    fwrite(file_buffer, 1, file_size, fp);

    fclose(fp);

    return 0;
}
void display_file(const char *filename)
{
    FILE *fp;
    char ch;

    fp = fopen(filename, "r");

    if (fp == NULL)
    {
        printf("Unable to open file\n");
        return;
    }

    printf("\n----- Shared File -----\n");

    while ((ch = fgetc(fp)) != EOF)
    {
        putchar(ch);
    }

    printf("\n-----------------------\n");

    fclose(fp);
}
void edit_file(const char *filename)
{
    FILE *fp;
    char text[1000];

    fp = fopen(filename, "w");

    if (fp == NULL)
    {
        printf("Unable to open file\n");
        return;
    }

    printf("\nEnter new content.\n");
    printf("Type END on a new line when finished.\n\n");

    while (1)
    {
        fgets(text, sizeof(text), stdin);

        if (strcmp(text, "END\n") == 0)
        {
            break;
        }

        fputs(text, fp);
    }

    fclose(fp);

    printf("File updated successfully\n");
}
void show_menu()
{
    printf("\n===== Collaborative File System =====\n");
    printf("1. View shared file\n");
    printf("2. Edit shared file\n");
    printf("3. Exit\n");
    printf("Enter your choice: ");
}
void clear_input_buffer()
{
    int ch;

    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }
}
int main()
{
    const char *filename = "shared.txt";
    int sockfd;
    struct sockaddr_in server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        printf("Socket creation failed\n");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Connection failed\n");
        close(sockfd);
        return 1;
    }

    printf("Connected to server\n");
    char welcome[100];

    if (receive_line(sockfd, welcome, sizeof(welcome)) < 0)
    {
      printf("Failed to receive welcome message\n");
      close(sockfd);
      return 1;
    }

    printf("Server: %s\n", welcome);
    int file_version;

    const char *request = "GET_FILE\n";

    send(sockfd, request, strlen(request), 0);

    if (receive_file(sockfd, &file_version) < 0)
    {
       printf("Failed to receive shared file from server\n");
       close(sockfd);
       return 1;
    }

    printf("Shared file received. Version: %d\n", file_version);
    int choice;

    do
    {
       show_menu();
       scanf("%d", &choice);
       clear_input_buffer();

       if (choice == 1)
       {
          display_file(filename);
       }
       else if (choice == 2)
       {
          edit_file(filename);
       }
       else if (choice == 3)
       {
          printf("Exiting...\n");
       }
      else
      {
          printf("Invalid choice\n");
      }

    } while (choice != 3);
    close(sockfd);

    return 0;
}
