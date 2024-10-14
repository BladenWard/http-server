#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return (EXIT_FAILURE);
    }
    printf("port: %s\n", argv[1]);

    return 0;
}
