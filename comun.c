#include "comun.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#endif


void responder(char solicitud, int campo, int valor, int socket_cliente){

    char mensaje[40];

    snprintf(mensaje,sizeof(mensaje),"%c&%d&%d\n",solicitud,campo,valor);
    send(socket_cliente, mensaje, strlen(mensaje), 0);
}

void enviarMensaje(int puerto_destino,char solicitud, int campo, int valor) {

    int sock;
    struct sockaddr_in direccion;
    char mensaje[40];
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("socket");
        return ;
    }
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto_destino);
    inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);
    if (connect(sock,(struct sockaddr *)&direccion,sizeof(direccion)) == -1) {
        close(sock);
        return ;
    }
    snprintf(mensaje,sizeof(mensaje),"%c&%d&%d\n",solicitud,campo,valor);
    send(sock, mensaje, strlen(mensaje), 0);
    close(sock);
}

int esperarRespuesta(int socket_cliente) {

    char buffer[100];
    char solicitud;
    int campo, valor;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    setsockopt(socket_cliente, SOL_SOCKET, SO_RCVTIMEO,&timeout, sizeof(timeout));

    int bytes = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);

    if (bytes > 0) {
        buffer[bytes] = '\0';
        if (sscanf(buffer, "%c&%d&%d",&solicitud, &campo, &valor) == 3 &&solicitud == 'A') {
            return 1;
        }
    }
    return 0;
}
