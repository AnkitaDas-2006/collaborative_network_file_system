#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
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
int main()
{
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

    close(sockfd);

    return 0;
}
