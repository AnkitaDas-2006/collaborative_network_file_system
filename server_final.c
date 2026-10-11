#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 3
#define MAX_FILES 50
#define MAX_FILE_SIZE 4096
#define MAX_NAME 100
#define MAX_LINE 512
#define MAX_HISTORY 1000
#define MAX_INSERT 400

typedef struct {
    char name[MAX_NAME];
    char content[MAX_FILE_SIZE + 1];
    int size;
    int revision;
} Document;

typedef struct {
    int type;       /* 1 = INSERT, 2 = DELETE */
    int position;
    int length;
    char text[MAX_INSERT + 1];
    int revision;
} Operation;

Document documents[MAX_FILES];
Operation history[MAX_FILES][MAX_HISTORY];

int history_count[MAX_FILES];
int document_count = 0;
int clients[MAX_CLIENTS];

int send_all(int fd, const char *data, int length)
{
    int total = 0;

    while (total < length) {
        int n = send(fd, data + total,
                     length - total, MSG_NOSIGNAL);

        if (n <= 0)
            return -1;

        total += n;
    }

    return 0;
}

int send_line(int fd, const char *line)
{
    return send_all(fd, line, strlen(line));
}

int recv_line(int fd, char *buffer, int size)
{
    int i = 0;

    while (i < size - 1) {
        char ch;
        int n = recv(fd, &ch, 1, 0);

        if (n <= 0)
            return n;

        if (ch == '\n')
            break;

        if (ch != '\r')
            buffer[i++] = ch;
    }

    buffer[i] = '\0';
    return i;
}

int recv_all(int fd, char *buffer, int size)
{
    int total = 0;

    while (total < size) {
        int n = recv(fd, buffer + total,
                     size - total, 0);

        if (n <= 0)
            return -1;

        total += n;
    }

    return 0;
}

int valid_name(const char *name)
{
    int n = strlen(name);

    if (n == 0 || n >= MAX_NAME ||
        strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0 ||
        strstr(name, "..") != NULL)
        return 0;

    for (int i = 0; i < n; i++) {
        if (!isalnum((unsigned char)name[i]) &&
            name[i] != '_' &&
            name[i] != '-' &&
            name[i] != '.')
            return 0;
    }

    return 1;
}

int find_document(const char *name)
{
    for (int i = 0; i < document_count; i++) {
        if (strcmp(documents[i].name, name) == 0)
            return i;
    }

    return -1;
}

/* Save the authoritative copy in documents/.
   Keep the root shared.txt synchronized as a mirror. */
int save_document(int index)
{
    char path[160];
    char temp_path[180];

    mkdir("documents", 0755);
    snprintf(path, sizeof(path), "documents/%s",
             documents[index].name);
    snprintf(temp_path, sizeof(temp_path),
             "documents/.tmp_%s", documents[index].name);

    FILE *fp = fopen(temp_path, "wb");
    if (fp == NULL)
        return -1;

    size_t written = fwrite(documents[index].content, 1,
                            documents[index].size, fp);
    int failed = ferror(fp);

    if (fclose(fp) != 0)
        failed = 1;

    if (failed || written != (size_t)documents[index].size) {
        remove(temp_path);
        return -1;
    }

    if (rename(temp_path, path) != 0) {
        remove(temp_path);
        return -1;
    }

    if (strcmp(documents[index].name, "shared.txt") == 0) {
        FILE *mirror = fopen("shared.txt", "wb");

        if (mirror != NULL) {
            size_t copied = fwrite(documents[index].content, 1,
                                   documents[index].size, mirror);
            int mirror_failed = ferror(mirror);

            if (fclose(mirror) != 0)
                mirror_failed = 1;

            if (mirror_failed ||
                copied != (size_t)documents[index].size)
                perror("Warning: could not fully update root shared.txt");
        } else {
            perror("Warning: could not update root shared.txt");
        }
    }

    return 0;
}

int save_index(void)
{
    FILE *fp = fopen("documents/.index", "w");

    if (fp == NULL)
        return -1;

    for (int i = 0; i < document_count; i++)
        fprintf(fp, "%s\n", documents[i].name);

    fclose(fp);
    return 0;
}

void load_documents(void)
{
    mkdir("documents", 0755);

    FILE *list = fopen("documents/.index", "r");

    if (list != NULL) {
        char name[MAX_NAME];

        while (fgets(name, sizeof(name), list) != NULL &&
               document_count < MAX_FILES) {
            name[strcspn(name, "\r\n")] = '\0';

            if (!valid_name(name) ||
                find_document(name) >= 0)
                continue;

            Document *d = &documents[document_count];
            memset(d, 0, sizeof(*d));
            strcpy(d->name, name);

            char path[160];
            snprintf(path, sizeof(path), "documents/%s", name);

            FILE *fp = NULL;

            if (strcmp(name, "shared.txt") == 0) {
                struct stat root_stat, document_stat;
                int root_exists = stat("shared.txt", &root_stat) == 0;
                int document_exists = stat(path, &document_stat) == 0;

                int root_is_newer = root_exists &&
                    (!document_exists ||
                     root_stat.st_mtim.tv_sec > document_stat.st_mtim.tv_sec ||
                     (root_stat.st_mtim.tv_sec == document_stat.st_mtim.tv_sec &&
                      root_stat.st_mtim.tv_nsec > document_stat.st_mtim.tv_nsec));

                fp = fopen(root_is_newer ? "shared.txt" : path, "rb");

                if (fp == NULL)
                    fp = fopen(root_is_newer ? path : "shared.txt", "rb");
            } else {
                fp = fopen(path, "rb");
            }

            if (fp == NULL) {
                fprintf(stderr, "Warning: skipping missing document %s\n",
                        name);
                continue;
            }

            d->size = fread(d->content, 1, MAX_FILE_SIZE, fp);
            int read_failed = ferror(fp);
            fclose(fp);

            if (read_failed) {
                fprintf(stderr, "Warning: could not read document %s\n",
                        name);
                continue;
            }

            d->content[d->size] = '\0';
            d->revision = 1;
            document_count++;
        }

        fclose(list);
    }

    if (document_count == 0) {
        Document *d = &documents[document_count];
        memset(d, 0, sizeof(*d));
        strcpy(d->name, "shared.txt");
        d->revision = 1;

        FILE *fp = fopen("shared.txt", "rb");

        if (fp != NULL) {
            d->size = fread(d->content, 1, MAX_FILE_SIZE, fp);
            fclose(fp);
        }

        d->content[d->size] = '\0';

        if (save_document(document_count) == 0)
            document_count++;
        else
            perror("Could not create shared.txt");
    }

    if (save_index() < 0)
        perror("Could not save file index");

    int shared_index = find_document("shared.txt");

    if (shared_index >= 0 &&
        save_document(shared_index) < 0)
        perror("Could not refresh documents/shared.txt");
}

void send_document(int fd, int index)
{
    Document *d = &documents[index];
    char header[200];

    snprintf(header, sizeof(header), "DOC %s %d %d\n",
             d->name, d->revision, d->size);

    if (send_line(fd, header) < 0)
        return;

    if (d->size > 0)
        send_all(fd, d->content, d->size);
}

void send_file_list(int fd)
{
    char header[100];

    snprintf(header, sizeof(header), "FILES %d\n",
             document_count);

    if (send_line(fd, header) < 0)
        return;

    for (int i = 0; i < document_count; i++) {
        char line[MAX_NAME + 2];

        snprintf(line, sizeof(line), "%s\n",
                 documents[i].name);

        if (send_line(fd, line) < 0)
            return;
    }
}

/* Transform an incoming operation against an accepted operation. */
void transform_operation(Operation *incoming,
                         const Operation *accepted)
{
    int p = incoming->position;
    int a = accepted->position;

    if (accepted->type == 1) {
        if (a < p ||
            (a == p && incoming->type == 1) ||
            (a == p && incoming->type == 2)) {
            incoming->position += accepted->length;
        }
    } else {
        int end = a + accepted->length;

        if (incoming->type == 1) {
            if (p >= end)
                incoming->position -= accepted->length;
            else if (p > a)
                incoming->position = a;
        } else {
            int b = p + incoming->length;

            if (b <= a) {
                /* Incoming deletion is before accepted deletion. */
            } else if (p >= end) {
                incoming->position -= accepted->length;
            } else {
                int start = p > a ? p : a;
                int finish = b < end ? b : end;
                int overlap = finish - start;

                if (overlap > 0)
                    incoming->length -= overlap;

                if (p > a)
                    incoming->position = a;
            }
        }
    }
}

void broadcast_operation(int sender, int index,
                         const Operation *op)
{
    char header[MAX_LINE];
    Document *d = &documents[index];

    if (op->type == 1) {
        snprintf(header, sizeof(header),
                 "OP %s %d %d INSERT %d %d\n",
                 d->name, d->revision, sender,
                 op->position, op->length);
    } else {
        snprintf(header, sizeof(header),
                 "OP %s %d %d DELETE %d %d\n",
                 d->name, d->revision, sender,
                 op->position, op->length);
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] < 0 || clients[i] == sender)
            continue;

        if (send_line(clients[i], header) < 0)
            continue;

        if (op->type == 1 && op->length > 0)
            send_all(clients[i], op->text, op->length);
    }
}

void handle_edit(int client, int index,
                 int base_revision, Operation *op)
{
    Document *d = &documents[index];

    if (base_revision < 1 || base_revision > d->revision) {
        send_line(client, "ERROR Invalid revision\n");
        return;
    }

    /* History entry 0 represents the change from revision 1 to 2. */
    int first = base_revision - 1;

    if (first > history_count[index]) {
        send_document(client, index);
        return;
    }

    if (history_count[index] >= MAX_HISTORY) {
        send_line(client, "ERROR History limit reached\n");
        return;
    }

    for (int i = first; i < history_count[index]; i++)
        transform_operation(op, &history[index][i]);

    if (op->position < 0 ||
        op->position > d->size ||
        op->length < 0) {
        send_line(client, "ERROR Invalid edit position\n");
        return;
    }

    if (op->type == 1) {
        if (op->length == 0 ||
            op->length > MAX_INSERT ||
            d->size + op->length > MAX_FILE_SIZE) {
            send_line(client, "ERROR Insert too large\n");
            return;
        }
    } else if (op->type == 2) {
        if (op->length > d->size - op->position) {
            send_line(client, "ERROR Delete out of range\n");
            return;
        }

        if (op->length == 0) {
            char ack[160];

            snprintf(ack, sizeof(ack), "ACK %s %d %d %d\n",
                     d->name, d->revision, op->position, 0);

            send_line(client, ack);
            return;
        }
    } else {
        send_line(client, "ERROR Unknown operation\n");
        return;
    }

    char old_content[MAX_FILE_SIZE + 1];
    int old_size = d->size;
    int old_revision = d->revision;

    memcpy(old_content, d->content, old_size + 1);

    if (op->type == 1) {
        memmove(d->content + op->position + op->length,
                d->content + op->position,
                d->size - op->position);

        memcpy(d->content + op->position,
               op->text, op->length);

        d->size += op->length;
        d->content[d->size] = '\0';
    } else {
        memmove(d->content + op->position,
                d->content + op->position + op->length,
                d->size - op->position - op->length);

        d->size -= op->length;
        d->content[d->size] = '\0';
    }

    d->revision++;
    op->revision = d->revision;

    if (save_document(index) < 0) {
        memcpy(d->content, old_content, old_size + 1);
        d->size = old_size;
        d->revision = old_revision;

        send_line(client, "ERROR Could not save document\n");
        return;
    }

    history[index][history_count[index]++] = *op;

    char ack[160];

    snprintf(ack, sizeof(ack), "ACK %s %d %d %d\n",
             d->name, d->revision,
             op->position, op->length);

    send_line(client, ack);
    broadcast_operation(client, index, op);

    printf("Accepted edit: %s revision %d\n",
           d->name, d->revision);
}

void handle_command(int client, const char *command)
{
    char name[MAX_NAME];
    int index;

    if (strcmp(command, "LIST") == 0) {
        send_file_list(client);
    }
    else if (sscanf(command, "CREATE %99s", name) == 1) {
        if (!valid_name(name)) {
            send_line(client, "ERROR Invalid filename\n");
            return;
        }

        if (find_document(name) >= 0) {
            send_line(client, "ERROR File already exists\n");
            return;
        }

        if (document_count >= MAX_FILES) {
            send_line(client, "ERROR Maximum files reached\n");
            return;
        }

        index = document_count;
        memset(&documents[index], 0, sizeof(Document));
        strcpy(documents[index].name, name);
        documents[index].revision = 1;

        if (save_document(index) < 0) {
            send_line(client, "ERROR Could not create file\n");
            return;
        }

        document_count++;

        if (save_index() < 0) {
            document_count--;
            send_line(client, "ERROR Could not save file list\n");
            return;
        }

        send_line(client, "CREATE_OK\n");
        send_document(client, index);
    }
    else if (sscanf(command, "OPEN %99s", name) == 1) {
        index = find_document(name);

        if (index < 0) {
            send_line(client, "ERROR File not found\n");
            return;
        }

        send_document(client, index);
    }
    else if (strncmp(command, "EDIT ", 5) == 0) {
        char type[20];
        char file_name[MAX_NAME];
        int base_revision, position, length;

        if (sscanf(command,
                   "EDIT %99s %d %19s %d %d",
                   file_name, &base_revision, type,
                   &position, &length) != 5) {
            send_line(client, "ERROR Invalid EDIT command\n");
            return;
        }

        index = find_document(file_name);

        if (index < 0) {
            send_line(client, "ERROR File not found\n");
            return;
        }

        Operation op;
        memset(&op, 0, sizeof(op));

        op.position = position;
        op.length = length;

        if (strcmp(type, "INSERT") == 0)
            op.type = 1;
        else if (strcmp(type, "DELETE") == 0)
            op.type = 2;
        else {
            send_line(client, "ERROR Invalid edit type\n");
            return;
        }

        if (op.type == 1) {
            if (length <= 0 || length > MAX_INSERT) {
                send_line(client, "ERROR Invalid insert length\n");
                return;
            }

            if (recv_all(client, op.text, length) < 0) {
                send_line(client, "ERROR Insert payload missing\n");
                return;
            }

            op.text[length] = '\0';
        }

        handle_edit(client, index, base_revision, &op);
    }
    else if (strcmp(command, "QUIT") == 0) {
        send_line(client, "BYE\n");
    }
    else {
        send_line(client, "ERROR Unknown command\n");
    }
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i] = -1;

    load_documents();

    int server = socket(AF_INET, SOCK_STREAM, 0);

    if (server < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               &reuse, sizeof(reuse));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(8080);

    if (bind(server, (struct sockaddr *)&address,
             sizeof(address)) < 0) {
        perror("bind");
        close(server);
        return 1;
    }

    if (listen(server, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server);
        return 1;
    }

    printf("Collaborative File Server running on 127.0.0.1:8080\n");
    printf("Loaded %d document(s).\n", document_count);

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);

        int maxfd = server;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] >= 0) {
                FD_SET(clients[i], &readfds);

                if (clients[i] > maxfd)
                    maxfd = clients[i];
            }
        }

        if (select(maxfd + 1, &readfds,
                   NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;

            perror("select");
            break;
        }

        if (FD_ISSET(server, &readfds)) {
            int fd = accept(server, NULL, NULL);

            if (fd >= 0) {
                int added = 0;

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] < 0) {
                        clients[i] = fd;
                        added = 1;
                        send_line(fd, "WELCOME\n");
                        printf("Client %d connected.\n", i + 1);
                        break;
                    }
                }

                if (!added) {
                    send_line(fd, "ERROR Server full\n");
                    close(fd);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] < 0 ||
                !FD_ISSET(clients[i], &readfds))
                continue;

            char command[MAX_LINE];

            int result = recv_line(clients[i], command,
                                   sizeof(command));

            if (result <= 0) {
                printf("Client %d disconnected.\n", i + 1);
                close(clients[i]);
                clients[i] = -1;
                continue;
            }

            handle_command(clients[i], command);
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] >= 0)
            close(clients[i]);
    }

    close(server);
    return 0;
}
