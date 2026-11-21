//
// Created by Gael on 21/11/2026.
//

#include "ficheros.h"
#include "comun.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PUERTO_A1 5002
#define PUERTO_B1 5101
#define PUERTO_C1 5201

int recibirMensaje(int sock, char *solicitud, int *campo, int *valor) {
    char buffer[100];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        return 0;
    }
    buffer[bytes] = '\0';
    return sscanf(buffer, "%c&%d&%d", solicitud, campo, valor) == 3;
}

int esperarAck(int sock) {
    for (int count = 0; count < 50; count++) {
        if (esperarRespuesta(sock)) {
            return 1;
        }
    }
    return 0;
}

void ejecutarTransaccion(Transaccion *tx) {
    struct sockaddr_in direccion;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("No se ha podido conectar con la capa %d\n", tx->capa);
        return;
    }

    int puerto;
    if (tx->capa == 0) {
        puerto = PUERTO_A1;
    }
    if (tx->capa == 1) {
        puerto = PUERTO_B1;
    }
    if (tx->capa == 2) {
        puerto = PUERTO_C1;
    }

    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);
    inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

    if (connect(sock, (struct sockaddr *)&direccion, sizeof(direccion)) == -1) {
        close(sock);
        return;
    }

    int abierta = 1;
    //TODO REWORKEAR ESTA PARTE
    for (int j = 0; j < tx->numOperaciones; j++) {
        if (!abierta) {
            break;
        }
        Operacion *op = &tx->operacion[j];

        if (op->tipo == 1) {
            char solicitud;
            int campo;
            int valor;

            responder('L', op->campo, 0, sock);
            if (recibirMensaje(sock, &solicitud, &campo, &valor) && solicitud == 'R') {
                printf("r(%d) = %d\n", op->campo, campo);
                responder('A', 0, 0, sock);
            } else {
                printf("sin respuesta a r(%d)\n", op->campo);
                break;
            }
        } else if (op->tipo == 2) {
            responder('W', op->campo, op->valor, sock);
            if (!esperarAck(sock)) {
                printf("sin ack de w(%d,%d)\n", op->campo, op->valor);
                break;
            }
            printf("w(%d,%d)\n", op->campo, op->valor);
        } else if (op->tipo == 3) {
            close(sock);
            abierta = 0;
        }
    }

    if (abierta == 1) {
        close(sock);
    }
}

int main(int argc, char *argv[]) {
    ListaTransacciones listaTransacciones;
    char *ruta;
    int count;

    if (argc != 2) {
        perror("Número incorrecto de argumentos\n");
        return 1;
    }

    ruta = argv[1];
    if (leerTransacciones(ruta, &listaTransacciones) < 0) {
        return 1;
    }

    for (count = 0; count < listaTransacciones.numTransacciones; count++) {
        ejecutarTransaccion(&listaTransacciones.transaccion[count]);
    }

    liberarTransacciones(&listaTransacciones);
    return 0;
}
