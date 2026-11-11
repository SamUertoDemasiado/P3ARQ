#ifndef COMUN_H
#define COMUN_H
    void responder(char solicitud, int campo, int valor, int socket_cliente);

    void enviarMensaje(int puerto_destino,char solicitud, int campo, int valor);

    int esperarRespuesta(int socket_cliente);
#endif