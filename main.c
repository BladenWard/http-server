#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

const char *find_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "jpg") == 0 ||
               strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

void make_http_response(const char *file_name, const char *file_ext,
                        char *response, size_t *response_size) {
    const char *mime_type = find_mime_type(file_ext);
    char *header = (char *)malloc(BUFSIZ * sizeof(char));
    snprintf(header, BUFSIZ,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             mime_type);

    int file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0) {
        snprintf(response, BUFSIZ,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        *response_size = strlen(header);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    *response_size = 0;
    memcpy(response, header, strlen(header));
    *response_size += strlen(header);

    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, response + *response_size, BUFSIZ)) >
           0) {
        *response_size += bytes_read;
    }

    free(header);
    close(file_fd);
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFSIZ * sizeof(char));

    ssize_t bytes_read = recv(client_fd, buffer, BUFSIZ, 0);
    if (bytes_read > 0) {
        regex_t regex;
        regcomp(&regex, "GET /([^ ]*) HTTP/1.1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0) {
            printf("request: %s\n", buffer);
            buffer[matches[1].rm_eo] = '\0';
            char *url_encoded_file_name = buffer + matches[1].rm_so;
            char *file_name = malloc(strlen(url_encoded_file_name) + 1);
            strcpy(file_name, "public/");
            decodeURIComponent(url_encoded_file_name, file_name);

            if (strcmp(file_name, "") == 0)
                strcpy(file_name, "public/index.html");

            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_name));

            char *response = (char *)malloc(BUFSIZ * 2 * sizeof(char));
            size_t response_size;
            make_http_response(file_name, file_ext, response, &response_size);

            send(client_fd, response, response_size, 0);
            free(file_name);
            free(response);
        }

        regfree(&regex);
    }
    close(client_fd);
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
