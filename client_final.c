#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
void show_menu(void);

#define PORT 8080
#define MAX_FILE_SIZE 4096
#define MAX_NAME 100
#define MAX_LINE 512
#define MAX_INSERT 400
#define MAX_FILES 50

int sock;
int disconnected = 0;

char current_name[MAX_NAME] = "";
char current_content[MAX_FILE_SIZE + 1] = "";
int current_size = 0;
int current_revision = 1;

int pending_edit = 0;
char pending_type[10] = "";
char pending_text[MAX_INSERT + 1] = "";
int pending_length = 0;

int response_kind = 0;
int response_done = 0;

int send_all(int fd, const char *buffer, int length)
{
    int sent = 0;

    while (sent < length) {
        int n = send(fd, buffer + sent, length - sent, 0);

        if (n <= 0)
            return -1;

        sent += n;
    }

    return 0;
}

int send_line(const char *line)
{
    char buffer[MAX_LINE];

    int n = snprintf(buffer, sizeof(buffer), "%s\n", line);

    if (n < 0 || (size_t)n >= sizeof(buffer))
        return -1;

    return send_all(sock, buffer, n);
}

int receive_line(int fd, char *buffer, int size)
{
    int i = 0;

    while (i < size - 1) {
        char ch;
        int n = recv(fd, &ch, 1, 0);

        if (n <= 0)
            return -1;

        if (ch == '\n')
            break;

        if (ch != '\r')
            buffer[i++] = ch;
    }

    buffer[i] = '\0';
    return i;
}

int receive_exact(int fd, char *buffer, int length)
{
    int received = 0;

    while (received < length) {
        int n = recv(fd, buffer + received,
                     length - received, 0);

        if (n <= 0)
            return -1;

        received += n;
    }

    return 0;
}

int valid_name(const char *name)
{
    int length = strlen(name);

    if (length == 0 || length >= MAX_NAME ||
        strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0 ||
        strstr(name, "..") != NULL)
        return 0;

    for (int i = 0; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) &&
            name[i] != '_' &&
            name[i] != '-' &&
            name[i] != '.')
            return 0;
    }

    return 1;
}

/* The server manages all permanent file storage. */
void save_local_file(void)
{
    /* Do not overwrite the server's shared.txt file. */
}

void display_file(void)
{
    if (current_name[0] == '\0') {
        printf("\nNo file is open. Open a file first.\n");
        return;
    }

    printf("\n========== %s ==========\n", current_name);
    printf("Revision: %d\n", current_revision);
    printf("------------------------------\n");

    if (current_size == 0)
        printf("(Empty file)\n");
    else
        fwrite(current_content, 1, current_size, stdout);

    printf("\n------------------------------\n");
}

void apply_insert(int position, const char *text, int length)
{
    if (position < 0 || position > current_size ||
        length < 0 ||
        current_size + length > MAX_FILE_SIZE) {
        printf("\nCannot apply insertion locally.\n");
        return;
    }

    memmove(current_content + position + length,
            current_content + position,
            current_size - position);

    memcpy(current_content + position, text, length);

    current_size += length;
    current_content[current_size] = '\0';
}

void apply_delete(int position, int length)
{
    if (position < 0 || position > current_size ||
        length < 0 || length > current_size - position) {
        printf("\nCannot apply deletion locally.\n");
        return;
    }

    memmove(current_content + position,
            current_content + position + length,
            current_size - position - length);

    current_size -= length;
    current_content[current_size] = '\0';
}

void process_server_message(void)
{
    char line[MAX_LINE];

    if (receive_line(sock, line, sizeof(line)) < 0) {
        disconnected = 1;
        printf("\nDisconnected from the server.\n");
        return;
    }

    if (strcmp(line, "WELCOME") == 0) {
        printf("\nConnected and ready.\n");
    }
    else if (strcmp(line, "CREATE_OK") == 0) {
        printf("\nFile created successfully.\n");
    }
    else if (strncmp(line, "DOC ", 4) == 0) {
        char name[MAX_NAME];
        int revision, size;

        if (sscanf(line, "DOC %99s %d %d",
                   name, &revision, &size) != 3 ||
            size < 0 || size > MAX_FILE_SIZE) {
            printf("\nInvalid document response.\n");
            disconnected = 1;
            return;
        }

        char buffer[MAX_FILE_SIZE + 1];

        if (receive_exact(sock, buffer, size) < 0) {
            disconnected = 1;
            return;
        }

        memcpy(current_content, buffer, size);
        current_content[size] = '\0';

        current_size = size;
        current_revision = revision;

        snprintf(current_name, sizeof(current_name), "%s", name);

        save_local_file();

        printf("\nOpened/synchronized file: %s\n", current_name);
        display_file();

        if (response_kind == 2)
            response_done = 1;
    }
    else if (strncmp(line, "FILES ", 6) == 0) {
        int count;

        if (sscanf(line, "FILES %d", &count) != 1 ||
            count < 0 || count > MAX_FILES) {
            printf("\nInvalid file list response.\n");
            response_done = 1;
            return;
        }

        printf("\n========== Available Files ==========\n");

        if (count == 0)
            printf("No files available.\n");

        for (int i = 0; i < count; i++) {
            char name[MAX_LINE];

            if (receive_line(sock, name, sizeof(name)) < 0) {
                disconnected = 1;
                return;
            }

            printf("%d. %s\n", i + 1, name);
        }

        printf("=====================================\n");

        if (response_kind == 1)
            response_done = 1;
    }
    else if (strncmp(line, "OP ", 3) == 0) {
        char name[MAX_NAME];
        char type[16];
        int revision, sender, position, length;

        if (sscanf(line, "OP %99s %d %d %15s %d %d",
                   name, &revision, &sender, type,
                   &position, &length) != 6) {
            printf("\nInvalid operation received.\n");
            return;
        }

        (void)sender;

        char inserted[MAX_INSERT + 1];

        if (strcmp(type, "INSERT") == 0) {
            if (length < 0 || length > MAX_INSERT) {
                disconnected = 1;
                return;
            }

            if (receive_exact(sock, inserted, length) < 0) {
                disconnected = 1;
                return;
            }

            inserted[length] = '\0';
        }

        if (strcmp(name, current_name) == 0 &&
            revision > current_revision) {

            if (revision != current_revision + 1) {
                printf("\nRevision gap detected. Reopen %s to resync.\n",
                       current_name);
                return;
            }

            if (strcmp(type, "INSERT") == 0)
                apply_insert(position, inserted, length);
            else if (strcmp(type, "DELETE") == 0)
                apply_delete(position, length);

            current_revision = revision;

            save_local_file();

            printf("\n[Live update] Revision %d\n", revision);
            display_file();
        }
    }
    else if (strncmp(line, "ACK ", 4) == 0) {
        char name[MAX_NAME];
        int revision, position, accepted_length;

        if (sscanf(line, "ACK %99s %d %d %d",
                   name, &revision, &position,
                   &accepted_length) != 4) {
            printf("\nInvalid acknowledgement.\n");
            pending_edit = 0;
            return;
        }

        if (pending_edit &&
            strcmp(name, current_name) == 0) {

            if (strcmp(pending_type, "INSERT") == 0) {
                apply_insert(position, pending_text,
                             accepted_length);
            }
            else if (strcmp(pending_type, "DELETE") == 0) {
                apply_delete(position, accepted_length);
            }

            current_revision = revision;

            save_local_file();

            printf("\nEdit accepted. Revision: %d\n",
                   current_revision);

            pending_edit = 0;
        }
    }
    else if (strncmp(line, "ERROR", 5) == 0) {
        printf("\nServer response: %s\n", line);

        pending_edit = 0;

        if (response_kind != 0)
            response_done = 1;
    }
    else if (strcmp(line, "BYE") == 0) {
        printf("\nServer closed the session.\n");
        disconnected = 1;
    }
    else {
        printf("\nServer: %s\n", line);
    }
}

int read_user_line(char *buffer, int size)
{
    int length = 0;

    while (!disconnected) {
        fd_set readfds;

        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        int maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;

        int result = select(maxfd + 1, &readfds,
                            NULL, NULL, NULL);

        if (result < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }


	if (FD_ISSET(sock, &readfds)) {
	    process_server_message();

	    if (disconnected)
	        return -1;

	    show_menu();
	}

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char ch;
            int n = read(STDIN_FILENO, &ch, 1);

            if (n <= 0)
                return -1;

            if (ch == '\n') {
                buffer[length] = '\0';
                return length;
            }

            if (length < size - 1)
                buffer[length++] = ch;
        }
    }

    return -1;
}

void wait_for_response(int kind)
{
    response_kind = kind;
    response_done = 0;

    while (!response_done && !disconnected) {
        fd_set readfds;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int result = select(sock + 1, &readfds,
                            NULL, NULL, NULL);

        if (result < 0) {
            if (errno == EINTR)
                continue;

            disconnected = 1;
            break;
        }

        if (FD_ISSET(sock, &readfds))
            process_server_message();
    }

    response_kind = 0;
    response_done = 0;
}

void wait_for_acknowledgement(void)
{
    while (pending_edit && !disconnected) {
        fd_set readfds;

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int result = select(sock + 1, &readfds,
                            NULL, NULL, NULL);

        if (result < 0) {
            if (errno == EINTR)
                continue;

            disconnected = 1;
            break;
        }

        if (FD_ISSET(sock, &readfds))
            process_server_message();
    }
}

void insert_text(void)
{
    if (current_name[0] == '\0') {
        printf("Open a file first.\n");
        return;
    }

    int position;

    printf("Enter insertion position (0 to %d): ", current_size);

    if (scanf("%d", &position) != 1) {
        while (getchar() != '\n')
            ;

        printf("Invalid position.\n");
        return;
    }

    while (getchar() != '\n')
        ;

    if (position < 0 || position > current_size) {
        printf("Position is outside the file.\n");
        return;
    }

    char text[MAX_INSERT + 1] = "";
    char line[MAX_INSERT + 2];
    int length = 0;

    printf("\nEnter text line by line.\n");
    printf("Type END on its own line when finished.\n");

    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL)
            return;

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, "END") == 0)
            break;

        int line_length = strlen(line);

        if (length + line_length + 1 > MAX_INSERT) {
            printf("Insertion limit is %d bytes. "
                   "Please enter a shorter block.\n",
                   MAX_INSERT);
            return;
        }

        memcpy(text + length, line, line_length);
        length += line_length;

        text[length++] = '\n';
        text[length] = '\0';
    }

    if (length == 0) {
        printf("Nothing to insert.\n");
        return;
    }

    if (current_size + length > MAX_FILE_SIZE) {
        printf("The file-size limit would be exceeded.\n");
        return;
    }

    char command[MAX_LINE];

    snprintf(command, sizeof(command),
             "EDIT %s %d INSERT %d %d",
             current_name, current_revision, position, length);

    pending_edit = 1;
    strcpy(pending_type, "INSERT");

    memcpy(pending_text, text, length);
    pending_text[length] = '\0';

    pending_length = length;

    if (send_line(command) < 0 ||
        send_all(sock, text, length) < 0) {
        pending_edit = 0;
        disconnected = 1;

        printf("Failed to send insertion.\n");
        return;
    }

    wait_for_acknowledgement();
}

void delete_text(void)
{
    if (current_name[0] == '\0') {
        printf("Open a file first.\n");
        return;
    }

    int position, length;

    printf("Enter deletion start position (0 to %d): ",
           current_size);

    if (scanf("%d", &position) != 1) {
        while (getchar() != '\n')
            ;

        printf("Invalid position.\n");
        return;
    }

    printf("Enter number of characters to delete: ");

    if (scanf("%d", &length) != 1) {
        while (getchar() != '\n')
            ;

        printf("Invalid length.\n");
        return;
    }

    while (getchar() != '\n')
        ;

    if (position < 0 || position > current_size ||
        length <= 0 || length > current_size - position) {
        printf("Invalid position or deletion length.\n");
        return;
    }

    char command[MAX_LINE];

    snprintf(command, sizeof(command),
             "EDIT %s %d DELETE %d %d",
             current_name, current_revision, position, length);

    pending_edit = 1;
    strcpy(pending_type, "DELETE");
    pending_length = length;

    if (send_line(command) < 0) {
        pending_edit = 0;
        disconnected = 1;
        return;
    }

    wait_for_acknowledgement();
}

void show_menu(void)
{
    printf("\n========== Collaborative File System ==========\n");
    printf("1. List files\n");
    printf("2. Create file\n");
    printf("3. Open file\n");
    printf("4. View current file\n");
    printf("5. Insert multi-line text\n");
    printf("6. Delete text\n");
    printf("7. Exit\n");

    if (current_name[0] != '\0')
        printf("Current file: %s (revision %d)\n",
               current_name, current_revision);
    else
        printf("No file currently open.\n");

    printf("================================================\n");
    printf("Enter choice: ");
    fflush(stdout);
}

int main(void)
{
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1",
                  &server_addr.sin_addr) != 1) {
        printf("Invalid server address.\n");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    printf("Connected to Collaborative File Server.\n");

    char welcome[MAX_LINE];

    if (receive_line(sock, welcome, sizeof(welcome)) < 0) {
        printf("Server did not send a welcome message.\n");
        close(sock);
        return 1;
    }

    printf("Server: %s\n", welcome);

    char input[MAX_LINE];

    while (!disconnected) {
        show_menu();

        if (read_user_line(input, sizeof(input)) < 0)
            break;

        if (strcmp(input, "1") == 0) {
            if (send_line("LIST") < 0)
                break;

            wait_for_response(1);
        }
        else if (strcmp(input, "2") == 0) {
            char name[MAX_NAME];
            char command[MAX_LINE];

            printf("Enter new file name (no spaces): ");

            if (fgets(name, sizeof(name), stdin) == NULL)
                break;

            name[strcspn(name, "\n")] = '\0';

            if (!valid_name(name)) {
                printf("Invalid filename.\n");
                continue;
            }

            snprintf(command, sizeof(command), "CREATE %s", name);

            if (send_line(command) < 0)
                break;

            wait_for_response(2);
        }
        else if (strcmp(input, "3") == 0) {
            char name[MAX_NAME];
            char command[MAX_LINE];

            printf("Enter file name to open: ");

            if (fgets(name, sizeof(name), stdin) == NULL)
                break;

            name[strcspn(name, "\n")] = '\0';

            if (!valid_name(name)) {
                printf("Invalid filename.\n");
                continue;
            }

            snprintf(command, sizeof(command), "OPEN %s", name);

            if (send_line(command) < 0)
                break;

            wait_for_response(2);
        }
        else if (strcmp(input, "4") == 0) {
            display_file();
        }
        else if (strcmp(input, "5") == 0) {
            insert_text();
        }
        else if (strcmp(input, "6") == 0) {
            delete_text();
        }
        else if (strcmp(input, "7") == 0) {
            send_line("QUIT");
            break;
        }
        else {
            printf("Invalid choice. Enter 1 to 7.\n");
        }
    }

    close(sock);
    printf("Client closed.\n");

    return 0;
}
