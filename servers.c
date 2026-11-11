#include "servers.h"
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
#include <unistd.h>
#endif

#define server_portA 5000
#define server_portB 5100
#define server_portC 5200
#define port_Client 5300


typedef struct {
    int info[100];
    int version;
    pthread_mutex_t mutex;
} Datos;


int conectarServidor(int puerto) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        return -1;
    }

    struct sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(puerto);

    inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);

    if (connect(sock, (struct sockaddr *)&direccion,
                sizeof(direccion)) == -1) {
        close(sock);
        return -1;
    }

    return sock;
}

int actualizarOtroCore(int puerto, int campo, int valor) {

    int sock = conectarServidor(puerto);

    if (sock == -1) {
        return 0;
    }
    while(1){
        responder('D', campo, valor, sock);
        if(esperarRespuesta(sock)){
            break;
        }
    }
    close(sock);
    return  1;
}

void ejecutarCore(int id){
    int version=0;
    int numero_socket_servidor=server_portA+id;
    int socket_servidor;
    int socket_cliente;
    int info[100];
    int new_ver;
    struct sockaddr_in direccion_servidor;
    struct sockaddr_in direccion_cliente;
    for(int i=0;i<100;i++){
        info[i]=0;
    }
    version=0;
    socklen_t tam_direccion_cliente;
    char buffer[40];
    //Crear socket
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_servidor == -1) {
        printf("Error creando socket");
        return;
    }

    printf("Socket creado\n");
    //Configurar la dirección del servidor
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(numero_socket_servidor);

    //Asociar socket al puerto
    int opt=1;
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("setsockopt");
        close(socket_servidor);
        return;
    }

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) == -1) {
        printf("Error en bind");
        close(socket_servidor);
        return;
    }

    //Escuchar
    if (listen(socket_servidor, 1) == -1) {
        printf("Error en listen");
        close(socket_servidor);
        return;
    }
    printf("Ejecutando Server A%d\n", id);
    char solicitud;
    int campo;
    int valor;
    while(1){
        new_ver=0;
        tam_direccion_cliente = sizeof(direccion_cliente);

        socket_cliente = accept(
            socket_servidor,(struct sockaddr *)&direccion_cliente,&tam_direccion_cliente
        );

        if (socket_cliente == -1) {
            continue;
        }
        int bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';
        }     

        sscanf(buffer, "%c&%d&%d", &solicitud, &campo, &valor);
        //piden lectura en servidor
        if (solicitud == 'L') {
            while (1) {
                responder('R', info[campo], 0, socket_cliente);
                if(esperarRespuesta(socket_cliente)){
                    break;
                }
            }
        //piden escritura en servidor
        }else if (solicitud == 'W'){
            new_ver=1;
            version++;
            info[campo]=valor;
            for (int port = 5001; port <= 5003; port++) {
                if (port == numero_socket_servidor) {
                    continue;
                }
                actualizarOtroCore(port, campo, valor);
            }
            responder('A', 0, 0, socket_cliente);
        }else if(solicitud == 'D'){
            new_ver = 1;
            version++; 
            info[campo]=valor;  
            responder('A', 0, 0, socket_cliente);
        }
        close(socket_cliente);  
        int puerto_conexion_lb;
        //se acaban de hacer 10 versiones
        if((version%10)==0 && new_ver == 1){
            if(numero_socket_servidor==5002){
                puerto_conexion_lb=server_portB+1;
            }else if(numero_socket_servidor==5003){
                puerto_conexion_lb=server_portB+2;
            }else{
                continue;
            }
            //conexion con su respectivo server de layer b y actualicacion
            int sock;
            struct sockaddr_in direccion;
            char mensaje[40];
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == -1) {
                printf("socket");
                return ;
            }
            direccion.sin_family = AF_INET;
            direccion.sin_port = htons(puerto_conexion_lb);
            inet_pton(AF_INET, "127.0.0.1", &direccion.sin_addr);
            if (connect(sock,(struct sockaddr *)&direccion,sizeof(direccion)) == -1) {
                close(sock);
                return ;
            }

            for(int a =0; a<100; a++){
                snprintf(mensaje,sizeof(mensaje),"%c&%d&%d\n",'D',a,info[a]);
                send(sock, mensaje, strlen(mensaje), 0);
                if(esperarRespuesta(sock)){
                    continue;
                }else{
                    a--;
                }
            }
            do {
                responder('V', 0, version, sock);
            } while (!esperarRespuesta(sock));

            close(sock);
            close(sock);
        }
    }
}

int enviarVersionLC(int puerto, int info[100], int version) {

    int sock = conectarServidor(puerto);

    if (sock == -1) {
        return 0;
    }
    while(1){
    responder('V', 0, version, sock);

    if (esperarRespuesta(sock) != 1) {
    }else{
        break;
    }
    }


    for (int a = 0; a < 100; a++) {

        responder('D', a, info[a], sock);

        if (esperarRespuesta(sock) != 1) {
            a--;
        }
    }

    close(sock);
    return 1;
}

void *actualizarC(void *arg) {

    Datos *datos = (Datos *)arg;
    while (1) {
        sleep(10);
        int copia[100];
        int version_copia;
        pthread_mutex_lock(&datos->mutex);
        memcpy(copia, datos->info, sizeof(copia));
        version_copia = datos->version;
        pthread_mutex_unlock(&datos->mutex);
        enviarVersionLC(5201, copia, version_copia);
        enviarVersionLC(5202, copia, version_copia);
    }
    return NULL;
}


void ejecutarLayerB(int id){
    int numero_socket_servidor=server_portB+id;
    int socket_servidor;
    int socket_cliente;
    struct sockaddr_in direccion_servidor;
    struct sockaddr_in direccion_cliente;
    Datos datos;
    for(int i=0;i<100;i++){
        datos.info[i]=0;
    }
    datos.version=0;
    pthread_mutex_init(&datos.mutex, NULL);

    if(numero_socket_servidor==5102){
        pthread_t hilo_actualizacion;

        pthread_create(&hilo_actualizacion, NULL, actualizarC, &datos);
    }

    socklen_t tam_direccion_cliente;
    char buffer[40];
    //Crear socket
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_servidor == -1) {
        printf("Error creando socket");
        return;
    }

    printf("Socket creado\n");
    //Configurar la dirección del servidor
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(numero_socket_servidor);

    //Asociar socket al puerto
    int opt=1;
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("setsockopt");
        close(socket_servidor);
        return;
    }

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) == -1) {
        printf("Error en bind");
        close(socket_servidor);
        return;
    }

    //Escuchar
    if (listen(socket_servidor, 1) == -1) {
        printf("Error en listen");
        close(socket_servidor);
        return;
    }
    printf("Ejecutando Server B%d\n", id);
    char solicitud;
    int campo;
    int valor;
    while(1){
        tam_direccion_cliente = sizeof(direccion_cliente);

        socket_cliente = accept(
            socket_servidor,(struct sockaddr *)&direccion_cliente,&tam_direccion_cliente
        );

        if (socket_cliente == -1) {
            continue;
        }
        int bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';
        }     

        sscanf(buffer, "%c&%d&%d", &solicitud, &campo, &valor);
        //piden lectura en servidor
        if (solicitud == 'L') {
            while (1) {
                responder('R', datos.info[campo], 0, socket_cliente);
                if(esperarRespuesta(socket_cliente)){
                    break;
                }
            }
        //actualizar version
        }else if (solicitud == 'D') {
            pthread_mutex_lock(&datos.mutex);
            while (1) {
                if (solicitud == 'D') {
                    datos.info[campo] = valor;
                    responder('A', 0, 0, socket_cliente);
                } else if (solicitud == 'V') {
                    datos.version = valor;
                    responder('A', 0, 0, socket_cliente);
                    break;
                } else {
                    break;
                }
                ssize_t recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
                if (recibidos <= 0) {
                    break;
                }
                buffer[recibidos] = '\0';
                sscanf(buffer, "%c&%d&%d",&solicitud, &campo, &valor);
            }
        pthread_mutex_unlock(&datos.mutex);
        }
    close(socket_cliente);  
    }
}

