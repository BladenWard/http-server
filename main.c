#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

uint PORT;

int decodeURIComponent(char *source, char *dest) {
    int length;
    for (length = 0; *source; length++) {
        if (*source == '%' && source[1] && source[2] && isxdigit(source[1]) &&
            isxdigit(source[2])) {
            source[1] -=
                source[1] <= '9' ? '0' : (source[1] <= 'F' ? 'A' : 'a') - 10;
            source[2] -=
                source[2] <= '9' ? '0' : (source[2] <= 'F' ? 'A' : 'a') - 10;
            dest[length] = 16 * source[1] + source[2];
            source += 3;
            continue;
        }
        dest[length] = *source++;
    }
    dest[length] = '\0';
    return length;
}

void *handle_client(void *cfd) {
    int client_fd = *((int *)cfd);
    char *buffer = (char *)malloc(BUFSIZ * sizeof(char));

    ssize_t bytes_read = recv(client_fd, buffer, BUFSIZ, 0);
    if (bytes_read > 0) {
        printf("buffer: %s\n", buffer);
        regex_t regex;
        regcomp(&regex, "GET /([^ ]*) HTTP/1.1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0) {
            buffer[matches[1].rm_eo] = '\0';
            char *url_encoded_file_name = buffer + matches[1].rm_so;
            char *file_name = malloc(strlen(url_encoded_file_name) + 1);
            decodeURIComponent(url_encoded_file_name, file_name);

            // TODO: read file in file_name and serve back to client
            printf("file_name: %s\n", file_name);

            char response[BUFSIZ];
            sprintf(response,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: %lu\r\n"
                    "\r\n%s",
                    strlen(file_name), file_name);
            size_t response_size = strlen(response);
            printf("response: %s\n", response);

            send(client_fd, response, response_size, 0);
        }

        regfree(&regex);
    }
    free(buffer);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    PORT = atoi(argv[1]);

    printf("starting server on port %d\n", PORT);
    int server_fd;
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                                 &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
    }

    return 0;
}
