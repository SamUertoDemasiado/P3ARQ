#include "comun.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int indiceValido(int indice) {
    return indice >= 0 && indice < N_CELDAS;
}

void responder(char solicitud, int campo, int valor, int socket_cliente) {
    char mensaje[40];
    snprintf(mensaje, sizeof(mensaje), "%c&%d&%d\n", solicitud, campo, valor);
    send(socket_cliente, mensaje, strlen(mensaje), 0);
}

int recibirLinea(int socket_fd, char *dest, int capacidad) {
    int count = 0;
    if (capacidad <= 1) {
        return -1;
    }

    while (count < capacidad - 1) {
        char charRecibido;
        int leidos;

        errno = 0;
        leidos = recv(socket_fd, &charRecibido, 1, 0);

        if (leidos <= 0) {
            return -1;
        }
        if (charRecibido == '\n') {
            dest[count] = '\0';
            return 1;
        }
        if (charRecibido != '\r') {
            dest[count++] = charRecibido;
        }
    }

    dest[count] = '\0';
    return 1;
}

int esperarRespuesta(int socket_cliente) {
    char buffer[100];
    char solicitud = 0;
    int campo = 0, valor = 0;
    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    setsockopt(socket_cliente, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int estado = recibirLinea(socket_cliente, buffer, sizeof(buffer));
    if (estado != 1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 2;
        }
        return -1;
    }

    sscanf(buffer, "%c&%d&%d", &solicitud, &campo, &valor);
    if (solicitud == 'A') {
        return 1;
    }
    return 0;
}

int esperarAck(int socket_cliente) {
    while (1) {
        int respuesta = esperarRespuesta(socket_cliente);

        if (respuesta == 1) {
            return 1;
        }
        if (respuesta == 2) {
            continue;
        }
        return 0;
    }
}

int conectarServidor(int puerto) {
    int sock = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in direccion;

    if (sock == -1) {
        return -1;
    }

    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);
    inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

    if (connect(sock, (struct sockaddr*)&direccion, sizeof(direccion)) == -1) {
        close(sock);
        return -1;
    }
    return sock;
}

int puertoNodoA(int id) {
    return 5000 + id * 2;
}

int puertoBssA(int id) {
    return puertoNodoA(id) + 1;
}

int puertoNodoB(int id) {
    return 5100 + id;
}

int puertoNodoC(int id) {
    return 5200 + id;
}
