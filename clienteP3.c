//
// Created by Gael on 21/11/2026.
//

#include "transacciones.h"
#include "comun.h"
#include "servers.h"

#include <stdio.h>
#include <unistd.h>

int leerEnNodo(int puerto, int campo, int *valor) {
    int campoResp = 0;
    char linea[80];
    char solicitud = 0;

    int sock = conectarServidor(puerto);
    if (sock == -1) {
        return 0;
    }

    responder('L', campo, 0, sock);
    recibirLinea(sock, linea, sizeof(linea));
    sscanf(linea, "%c&%d&*d", &solicitud, &campoResp);
    if (solicitud != 'R') {
        close(sock);
        return 0;
    }

    *valor = campoResp;
    responder('A', 0, 0, sock);
    close(sock);
    return 1;
}

int escribirEnNodo(int puerto, int campo, int valor) {
    int sock = conectarServidor(puerto);
    if (sock == -1) {
        return 0;
    }
    responder('W', campo, valor, sock);
    if (esperarAck(sock) == 1) {
        close(sock);
        return 1;
    }
    close(sock);
    return 0;
}

void ejecutarTransaccion(Transaccion *transaccion) {
    int puerto;
    int capa = transaccion->capa;

    if (!transaccion->soloLectura) {
        if (capa != 0) {
            printf("La escritura se ha de ejecutar obligatoriamente en la capa A\n");
        }
        capa = 0;
    }
    if (capa == 1) {
        puerto = puertoNodoB(1);
        printf("Transaccion en la capa B\n");
    } else if (capa == 2) {
        puerto = puertoNodoC(1);
        printf("Transaccion en la capa C\n");
    } else {
        puerto = puertoNodoA(1);
        printf("Transaccion en la capa A (core)\n");
    }


    for (int j = 0; j < transaccion->numOperaciones; j++) {
        Operacion *operacion = &transaccion->operacion[j];

        if (operacion->tipo == 0) {
            continue;
        }
        if (operacion->tipo == 3) {
            printf("c\n");
            break;
        }
        if ((operacion->tipo == 1 || operacion->tipo == 2) && !indiceValido(operacion->campo)) {
            printf("celda %d fuera de rango (0-%d)\n", operacion->campo, N_CELDAS - 1);
            return;
        }
        if (operacion->tipo == 1) {
            int valor;
            if (!leerEnNodo(puerto, operacion->campo, &valor)) {
                printf("sin respuesta a r(%d)\n", operacion->campo);
                return;
            }
            printf("r(%d) = %d\n", operacion->campo, valor);
        } else if (operacion->tipo == 2) {
            if (!escribirEnNodo(puerto, operacion->campo, operacion->valor)) {
                printf("sin ack de w(%d,%d)\n", operacion->campo, operacion->valor);
                return;
            }
            printf("w(%d,%d)\n", operacion->campo, operacion->valor);
        }
    }
}

int main(int argc, char *argv[]) {
    ListaTransacciones listaTransacciones;
    char *ruta;

    if (argc != 2) {
        printf("Uso: %s <fichero de transacciones>\n", argv[0]);
        return 1;
    }

    ruta = argv[1];
    if (leerTransacciones(ruta, &listaTransacciones) < 0) {
        return 1;
    }

    for (int count = 0; count < listaTransacciones.numTransacciones; count++) {
        ejecutarTransaccion(&listaTransacciones.transaccion[count]);
    }

    liberarTransacciones(&listaTransacciones);
    return 0;
}
