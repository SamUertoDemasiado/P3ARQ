#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "servers.h"

int main(int argc, char *argv[]) {
    char *id;
    int number;

    if (argc != 3) {
        printf("ERROR - Formato: %s <A|B|C> <id>\n", argv[0]);
        return 1;
    }

    id = argv[1];
    number = atoi(argv[2]);
    if (id[0] == 'A') {
        ejecutarCore(number);
    }
    else if (id[0] == 'B') {
        ejecutarLayerB(number);
    }
    else if (id[0] == 'C') {
        ejecutarLayerC(number);
    }

    return 0;
}

