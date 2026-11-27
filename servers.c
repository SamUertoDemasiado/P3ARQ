#include "servers.h"
#include "comun.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

typedef struct {
    int info[N_CELDAS];
    int version;
    int Ta[3];
    int id;
    pthread_mutex_t mutex;
} Capa;

int puedeEntregar(int local[3], int mensaje[3]) {
    int diferencias = 0;

    for (int i = 0; i < 3; i++) {
        if (mensaje[i] == local[i] + 1) {
            diferencias++;
        } else if (mensaje[i] != local[i]) {
            return 0;
        }
    }
    return diferencias == 1;
}

int yaEntregado(int local[3], int mensaje[3]) {
    for(int i = 0; i < 3; i++) {
        if (mensaje[i] > local[i]) {
            return 0;
        }
    }
    return 1;
}

int abrirServidor(int puerto) {
    struct sockaddr_in direccion;
    int servidor = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;

    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&direccion, 0, sizeof(direccion));

    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(puerto);
    bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion));
    listen(servidor, 8);
    return servidor;
}

void guardarEscritura(int id, int version, int pos, int valor, int Ta[3]) {
    char nombre[32];
    FILE *fichero;

    snprintf(nombre, sizeof(nombre), "logA%d.txt", id);
    fichero = fopen(nombre, "a");
    fprintf(fichero,"Versión %d:\n\tÚltima celda actualizada %d con valor %d\n\tEscrituras por nodo - Nodo 1: %d Nodo 2: %d Nodo 3: %d\n", version, pos, valor, Ta[0], Ta[1], Ta[2]);
    fclose(fichero);
}

void guardarSnapshot(char capa, int id, int version, int info[N_CELDAS]) {
    char nombre[32];
    FILE *fichero;

    snprintf (nombre, sizeof(nombre), "log%c%d.txt", capa, id);
    fichero = fopen(nombre, "a");
    fprintf(fichero, "Historial de versión %d:\n\t[", version);
    for (int i = 0; i < N_CELDAS; i++) {
        fprintf(fichero, " %d", info[i]);
    }
    fprintf(fichero, "]\n");
    fclose(fichero);
}

void anunciarMonitor(char capa, int id, int version, int pos, int valor) {
    char mensaje[120];
    int sock;

    snprintf(mensaje, sizeof(mensaje), "{\"nodo\":\"%c%d\",\"version\":%d,\"pos\":%d,\"valor\":%d}\n", capa, id, version, pos, valor);
    sock=conectarServidor(8090);
    send(sock, mensaje, strlen(mensaje), 0);
    close(sock);
}

void actualizarOtroCore(int puerto, int campo, int valor, int Ta[3]) {
    char mensaje[80];

    snprintf(mensaje, sizeof(mensaje), "D&%d&%d&%d&%d&%d\n", Ta[0], Ta[1], Ta[2], campo, valor);
    while (1) {
        int sock = conectarServidor(puerto);
        send(sock, mensaje, strlen(mensaje), 0);
        if (esperarAck(sock) == 1) {
            close(sock);
            return;
        }
        close(sock);
        usleep(100000);
    }
}

void enviarSnapshot(int puerto, int info[N_CELDAS], int version) {
    int sock = conectarServidor(puerto);

    for (int i = 0; i < N_CELDAS; i++) {
        responder('D', i, info[i], sock);
        if (esperarAck(sock) != 1) {
            i--;
        }
    }
    responder('V', 0, version, sock);
    while (esperarAck(sock) != 1) {
        usleep(100000);
    }
    close(sock);
}

void enviarHaciaB(int id, int info[N_CELDAS], int version) {
    int puerto;

    if (version == 0 || version % 10 != 0) {
        return;
    }
    if (id == 2) {
        puerto = puertoNodoB(1);
        printf("Nodo A%d envia la version %d a B1\n",id,version);
    } else if (id == 3) {
        puerto = puertoNodoB(2);
        printf("Nodo A%d envia la version %d a B2\n",id,version);
    }
    else {
        return;
    }
    enviarSnapshot(puerto, info, version);
}

void *fBSS(void *arg) {
    Capa *datos = (Capa*)arg;
    int servidor = abrirServidor(puertoBssA(datos->id));

    while (1) {
        struct sockaddr_in dir;
        char solicitud;
        char linea[80];
        int vector[3];
        int copia[N_CELDAS];
        int t0, t1, t2, pos, val;
        int version = 0, entregar = 0, duplicado = 0;

        socklen_t tam = sizeof(dir);
        int cliente = accept(servidor, (struct sockaddr *)&dir, &tam);

        recibirLinea(cliente, linea, sizeof(linea));
        sscanf(linea, "%c&%d&%d&%d&%d&%d", &solicitud, &t0, &t1, &t2, &pos, &val);
        vector[0] = t0;
        vector[1] = t1;
        vector[2] = t2;

        pthread_mutex_lock(&datos->mutex);
        if (yaEntregado(datos->Ta, vector)) {
            duplicado = 1;
        } else if (puedeEntregar(datos->Ta, vector)) {
            datos->Ta[0] = vector[0];
            datos->Ta[1] = vector[1];
            datos->Ta[2] = vector[2];
            if (indiceValido(pos)) {
                datos->info[pos] = val;
            }
            version = datos->Ta[0] + datos->Ta[1] + datos->Ta[2];
            memcpy(copia, datos->info, sizeof(copia));
            entregar = 1;
        }
        pthread_mutex_unlock(&datos->mutex);

        if (duplicado || entregar) {
            responder('A', 0, 0, cliente);
        } else {
            responder('S', 0, 0, cliente);
        }
        close(cliente);

        if (entregar) {
            guardarEscritura(datos->id, version, pos, val, vector);
            anunciarMonitor('A', datos->id, version, pos, val);
            enviarHaciaB(datos->id, copia, version);
        }
    }
}

void ejecutarCore(int id) {
    Capa datos = {0};
    pthread_t hilo;
    int servidor;

    signal(SIGPIPE, SIG_IGN);
    datos.id = id;
    pthread_mutex_init(&datos.mutex, NULL);
    pthread_create(&hilo, NULL, fBSS, &datos);
    anunciarMonitor('A', id, 0, -1, 0);

    servidor = abrirServidor(puertoNodoA(id));
    printf("Ejecutando Server A%d \n", id);

    while (1) {
        struct sockaddr_in dir;
        socklen_t tam = sizeof(dir);
        char linea[80];
        char solicitud;
        int campo, valor;
        int cliente = accept(servidor, (struct sockaddr *)&dir, &tam);

        recibirLinea(cliente, linea, sizeof(linea));
        sscanf(linea, "%c&%d&%d", &solicitud, &campo, &valor);
        if ((solicitud == 'L' || solicitud == 'W') && !indiceValido(campo)) {
            printf("A%d rechaza la celda %d\n", id, campo);
            close(cliente);
            continue;
        }

        if (solicitud == 'L') {
            int valorLeido;

            pthread_mutex_lock(&datos.mutex);
            valorLeido = datos.info[campo];
            pthread_mutex_unlock(&datos.mutex);
            responder('R', valorLeido, 0, cliente);
            esperarAck(cliente);
        } else if (solicitud == 'W') {
            int vector[3];
            int copia[N_CELDAS];
            int version;

            pthread_mutex_lock(&datos.mutex);
            datos.Ta[id - 1]++;
            datos.info[campo] = valor;
            version = datos.Ta[0] + datos.Ta[1] + datos.Ta[2];
            memcpy(vector, datos.Ta, sizeof(vector));
            memcpy(copia, datos.info, sizeof(copia));
            pthread_mutex_unlock(&datos.mutex);

            for (int otro = 1; otro <= 3; otro++) {
                if (otro != id) {
                    actualizarOtroCore(puertoBssA(otro), campo, valor, vector);
                }
            }

            guardarEscritura(id, version, campo, valor, vector);
            printf("Nueva version nodo A%d: version %d\n\tCelda %d Valor %d\n", id, version, campo, valor);
            responder('A', 0, 0, cliente);
            anunciarMonitor('A', id, version, campo, valor);
            enviarHaciaB(id, copia, version);
        }
        close(cliente);
    }
}

void atenderConexion(Capa *datos, int socket_cliente, char capa, int id) {
    char linea[80];
    char solicitud;
    int campo, valor;
    int temp[N_CELDAS];
    int versionRecibida;

    recibirLinea(socket_cliente, linea, sizeof(linea));
    sscanf(linea, "%c&%d&%d", &solicitud, &campo, &valor);

    if (solicitud == 'L') {
        int valorLeido;

        if (!indiceValido(campo)) {
            printf("%c%d rechaza la celda %d\n", capa, id, campo);
            return;
        }
        pthread_mutex_lock(&datos->mutex);
        valorLeido = datos->info[campo];
        pthread_mutex_unlock(&datos->mutex);
        responder('R', valorLeido, 0, socket_cliente);
        esperarAck(socket_cliente);
        return;
    }

    pthread_mutex_lock(&datos->mutex);
    memcpy(temp, datos->info, sizeof(temp));
    pthread_mutex_unlock(&datos->mutex);

    while (solicitud == 'D') {
        if (indiceValido(campo)) {
            temp[campo] = valor;
        }
        responder('A', 0, 0, socket_cliente);
        recibirLinea(socket_cliente, linea, sizeof(linea));
        sscanf(linea, "%c&%d&%d", &solicitud, &campo, &valor);
    }

    versionRecibida = valor;
    responder('A', 0, 0, socket_cliente);

    pthread_mutex_lock(&datos->mutex);
    if (versionRecibida == datos->version) {
        pthread_mutex_unlock(&datos->mutex);
        return;
    }
    memcpy(datos->info, temp, sizeof(temp));
    datos->version = versionRecibida;
    pthread_mutex_unlock(&datos->mutex);

    guardarSnapshot(capa, id, versionRecibida, temp);
    printf("%c%d instala la version %d\n", capa, id, versionRecibida);
    anunciarMonitor(capa, id, versionRecibida, -1, 0);
}

void *actualizarC(void *arg) {
    Capa *datos = (Capa *)arg;

    while (1) {
        int copia[N_CELDAS];
        int version;

        sleep(10);
        pthread_mutex_lock(&datos->mutex);
        memcpy(copia, datos->info, sizeof(copia));
        version = datos->version;
        pthread_mutex_unlock(&datos->mutex);
        if (version == 0) {
            continue;
        }
        enviarSnapshot(puertoNodoC(1), copia, version);
        enviarSnapshot(puertoNodoC(2), copia, version);
        printf("B2 envia la version %d a C1 y C2\n", version);
    }
}

void ejecutarCapa(char capa, int id) {
    Capa datos;
    int puerto, servidor;

    signal(SIGPIPE, SIG_IGN);
    pthread_mutex_init(&datos.mutex, NULL);
    if (capa == 'B'){
        puerto = puertoNodoB(id);
    }
    else {
        puerto = puertoNodoC(id);
    }
    servidor = abrirServidor(puerto);
    printf("Ejecutando Server %c%d\n", capa, id);
    anunciarMonitor(capa, id, 0, -1, 0);
    if (capa == 'B' && id == 2) {
        pthread_t hilo;
        pthread_create(&hilo, NULL, actualizarC, &datos);
    }

    while (1) {
        struct sockaddr_in dir;
        socklen_t tam = sizeof(dir);
        int cliente = accept(servidor, (struct sockaddr*)&dir, &tam);
        atenderConexion(&datos, cliente, capa, id);
        close(cliente);
    }
}

void ejecutarLayerB(int id) {
    ejecutarCapa('B', id);
}

void ejecutarLayerC(int id) {
    ejecutarCapa('C', id);
}
