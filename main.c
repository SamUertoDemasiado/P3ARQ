#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
//#endif
#include "servers.h"

void *lowRichard(void *arg) {
    

    int puerto = *(int *)arg;
    int socket_servidor;
    int socket_cliente;
    struct sockaddr_in direccion_servidor;
    struct sockaddr_in direccion_cliente;
    int timestamp=0;

    socklen_t tam_direccion_cliente;
    char buffer[40];
    //Crear socket
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_servidor == -1) {
        printf("Error creando socket");
        return NULL;
    }

    printf("Socket creado\n");
    //Configurar la dirección del servidor
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(puerto);

    //Asociar socket al puerto
    int opt = 1;
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("setsockopt");
        close(socket_servidor);
        return NULL;
    }

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) == -1) {
        printf("Error en bind");
        close(socket_servidor);
        return NULL;
    }

    //Escuchar
    if (listen(socket_servidor, 3) == -1) {
        printf("Error en listen");
        close(socket_servidor);
        return NULL;
    }

    int port;
    int type;
    int solicitud=0;
    int newtime;
    int cola[3];
    int id_cola=0;
    int ack_totales=0;
    int timestamprequest;
        printf("Ejecutando User Richard %d\n",puerto-5000);
    while(1){
        tam_direccion_cliente = sizeof(direccion_cliente);

        socket_cliente = accept(
            socket_servidor,(struct sockaddr *)&direccion_cliente,&tam_direccion_cliente //rcibir fin low
        );

        if (socket_cliente == -1) {
            continue;
        }

        int bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recibidos <= 0) {
            close(socket_cliente);
            continue;
        }
        
        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0';
        }
        sscanf(buffer, "%d&%d&%d", &port,&type,&newtime);
        close(socket_cliente);  
        /*
        Server-server
            1=tu turno de token
        Server-cliente
            1=puedes empezar
        cliente-Server
            1=he terminado
        cliente-cliente
            1=Rquest
            2=ACK
        */

        if(port==5000){
            //Server-cliente
            solicitud=1;
            timestamp++;
            timestamprequest=timestamp;
            for (int i=5001; i<5004; i++){
                if(i==puerto){
                }else{
                    enviarMensaje(i,puerto,1,timestamp);
                }
            }
        }else{
            if(type==1){//piden ack
                if(solicitud==0){//piden ack y el aun no ha pedido de entrar
                    if(newtime>timestamp){
                        timestamp=newtime;
                    }
                    timestamp++;
                    enviarMensaje(port,puerto,2,timestamp);
                }else{//piden ack y si quiero entrar
                    if(newtime>timestamprequest){//su tiempo es mayor, almacenamos y actualizamos tiempo
                        timestamp=newtime;
                        timestamp++;
                        cola[id_cola]=port;
                        id_cola++;
                    }else if(newtime<timestamprequest){ //su tiempo es menor: reply
                        timestamp++;
                        enviarMensaje(port,puerto,2,timestamp);
                    }else if(port<puerto){//tiempos == pero id menor?
                        timestamp++;
                        enviarMensaje(port,puerto,2,timestamp);
                    }else{//id mayor?
                        timestamp++;
                        cola[id_cola]=port;
                        id_cola++;
                    }
                }
            }else{//me dan un ACK
                if(newtime>timestamp){
                    timestamp=newtime;
                }
                timestamp++;
                ack_totales++;
                if(ack_totales>=2){//puedo utilizar pantalla
                    int myID = puerto-5000;
                    for (int i = 0; i < 10; i++) {//printar por pantalla
                        printf("I'm lightweight process B%d\n", myID);
                        usleep(250000);
                    }
                    timestamp++;
                    for (int i = 0; i < id_cola; i++) {//liberar cola
                        enviarMensaje(cola[i],puerto,2,timestamp); 
                        cola[i]=0;
                    }
                    //avisar al servidor + reiniciar valores
                    enviarMensaje(5000,puerto,1,timestamp); 
                    solicitud=0;
                    id_cola=0;
                    ack_totales=0;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {

    char *id = argv[1];
    int number = atoi(argv[2]);
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

