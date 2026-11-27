#ifndef COMUN_H
#define COMUN_H

#define N_CELDAS 10

void responder(char solicitud, int campo, int valor, int socket_cliente);
int indiceValido(int indice);
int recibirLinea(int socket_fd, char *dest, int capacidad);

/* 1 = A, 0 = otra respuesta, 2 = timeout, -1 = socket cerrado */
int esperarRespuesta(int socket_cliente);

/* 1 = ha llegado A. 0 = NACK o socket cerrado. El timeout sigue esperando. */
int esperarAck(int socket_cliente);
int conectarServidor(int puerto);
int puertoNodoA(int id);
int puertoBssA(int id);
int puertoNodoB(int id);
int puertoNodoC(int id);

#endif
