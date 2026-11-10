#define _GNU_SOURCE
#include <stdio.h> // para snprintf y asprintf
#include <stdlib.h> // para atoi
#include <string.h>// para strlen
#include <unistd.h>// para read y write
#include <fcntl.h>// para read y write
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#define ip "127.0.0.1"
#define LAMPORT_CONNECT_HEAVY_PORT 8080
#define RICHARD_HEAVY_PORT 5000
#define LAMPORT_CONNECT_LIGHT_PORT 8070
#define NUM_LIGHTWEIGHTS 3

int light_fd[NUM_LIGHTWEIGHTS];

char *readUntil(int file, char end) {
    char *s = NULL;
    char c;
    int i = 0, n;

    while ((n = read(file, &c, 1)) > 0 && c != end) {
        s = realloc(s, i + 2);
        s[i++] = c;
    }

    if (n <= 0 && i == 0) {
        return NULL;
    }

    s[i] = '\0';
    return s;
}

int connectToPort(int port) {
    struct sockaddr_in s_addr;
    int fd_socket = -1;

    fd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);// creamos el socket para comunicarnos por TCP
    if (fd_socket < 0) {// si el socket se ha creado bien, se salta este if
        printf("ERROR: Error en la creación del socket cliente.\n");// si no se ha podido crear, sale el error
        return -1;
    }

    memset(&s_addr, 0, sizeof(s_addr));// ponemos toda la estructura a cero
    s_addr.sin_family = AF_INET; // versión 4
    s_addr.sin_port = htons(port);// se pasa a big endian para tratarlo bien

    if (inet_pton(AF_INET, ip, &s_addr.sin_addr) < 0) { // pasamos la IP a big endian
        printf("ERROR: Error con la IP del socket cliente.\n");// no se ha podido preparar la IP
        close(fd_socket);// cerramos el socket
        return -1;
    }

    if (connect(fd_socket, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) { // no conecta si la IP o el puerto no son correctos
        printf("ERROR: Error en la conexión del socket cliente.\n");
        close(fd_socket);
        return -1;
    }

    return fd_socket;
}

int listenOnPort(int listen_port) {
    struct sockaddr_in s_addr;
    int fd_socket = -1;

    fd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);// creamos el socket para comunicarnos por TCP
    if (fd_socket < 0) {// si el socket se ha creado bien, se salta este if
        printf("ERROR: Error en la creación del socket high lamport.\n");// si no se ha podido crear, sale el error
        return -1;
    }

    memset(&s_addr, 0, sizeof(s_addr));// ponemos toda la estructura a cero
    s_addr.sin_family = AF_INET; // versión 4
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(listen_port);// se pasa a big endian para tratarlo bien

    if (bind(fd_socket, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
        perror("bind");
        close(fd_socket);
        return -1;
    }
    if (listen(fd_socket, 8) < 0) {
        perror("listen");
        close(fd_socket);
        return -1;
    }
    return fd_socket;
}

int forkLightProcesses(void) {
    int listen_fd = listenOnPort(LAMPORT_CONNECT_LIGHT_PORT);
    if (listen_fd < 0) {
        return -1;
    }
    //fcntl(listen_fd, F_SETFD, FD_CLOEXEC);

    for (int i = 1; i <= NUM_LIGHTWEIGHTS; i++) {
        light_fd[i-1] = -1;
        int pid = fork();
        char buffer[16];

        // error en el fork
        if (pid < 0) {
            perror("fork");
            close(listen_fd);
            return -1;
        }
        // el hijo pasa a ser el light
        if (pid == 0) {
            snprintf(buffer, sizeof(buffer), "%d", i);
            execl("./P2LightLamport", "P2LightLamport", buffer, (char *) NULL);
            // execl no vuelve si ha ido bien
            perror("execl");
            return -1;
        }
        printf("Light Lamport-%d iniciado (pid %d)\n", i, pid);
    }

    // esperamos la primera respuesta de cada light para guardar su descriptor
    int count = 0;
    while (count < NUM_LIGHTWEIGHTS) {
        int fd = accept(listen_fd, NULL, NULL);
        if (fd < 0) {
            perror("accept");
            close(listen_fd);
            return -1;
        }

        int id = 0;
        char *msg = readUntil(fd, '\n');
        if (msg != NULL && sscanf(msg, "%d", &id) == 1) {
            light_fd[id - 1] = fd;
            count++;
        } else {
            close(fd);
        }
        free(msg);
    }

    close(listen_fd);
    return 0;
}

int lightProcess(void) {
    // enviamos "S" para que los lights empiecen
    for (int i = 0; i < NUM_LIGHTWEIGHTS; i++) {
        if (write(light_fd[i], "S\n", 2) < 0) {
            return -1;
        }
    }

    // esperamos el "D" de cada light al terminar
    for (int i = 0; i < NUM_LIGHTWEIGHTS; i++) {
        char *msg = readUntil(light_fd[i], '\n');
        if (msg == NULL || strcmp(msg, "D") != 0) {
            free(msg);
            return -1;
        }
        free(msg);
    }
    return 0;
}

int main(void) {
    char *message = NULL;
    if (forkLightProcesses() < 0) {
        printf("ERROR: no se han podido crear los light processes.\n");
        return 1;
    }

    /* Richard conecta a 8080 para entregar el token. Este proceso escucha ahí. */
    int heavy_listen = listenOnPort(LAMPORT_CONNECT_HEAVY_PORT);
    if (heavy_listen < 0) {
        printf("ERROR: no se ha podido escuchar el puerto del otro heavy.\n");
        return 1;
    }

    while(1){
        int fd_socket = accept(heavy_listen, NULL, NULL);
        if (fd_socket < 0) {
            perror("accept");
            break;
        }

        message = readUntil(fd_socket, '\n'); // esperamos el token del heavy de Richard
        close(fd_socket);
        // recibimos el token del otro heavy
        if(message != NULL && strcmp(message, "5000&1&1") == 0){
            printf("LAMPORT LIGHT START\n");
            if (lightProcess() < 0) {
                printf("ERROR: los light processes no han respondido.\n");
                free(message);
                break;
            }
            free(message);
        }
        else{
            printf("ERROR RECIBIENDO MENSAJE EN HIGH LAMPORT (1)\n");
            free(message);
        }

        // enviamos el token al otro heavy
        fd_socket = connectToPort(RICHARD_HEAVY_PORT);
        if (fd_socket < 0) {
            printf("\nLAMPORT no se ha podido conectar por problemas de configuración...\n");
            break;
        }
        write(fd_socket, "4000",strlen("4000")); // enviamos el token al heavy de Richard
        close(fd_socket);
        printf("LAMPORT TOKEN SEND\n");
    }

    return 0;
}

