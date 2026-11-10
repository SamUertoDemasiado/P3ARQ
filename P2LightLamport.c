#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define LAMPORT_CONNECT_HEAVY_PORT 8070
#define LAMPORT_LIGHT_PORT 8100
#define NUM_LIGHTWEIGHTS 3

typedef struct {
    int timestamp;
    int id;
    int valid;
} Message;

typedef struct {
    int id;
    char my_id[8];
    int clock;
    int heavy_fd;
    int listen_fd;
    int light_fd[NUM_LIGHTWEIGHTS + 1];
    int reply_from[NUM_LIGHTWEIGHTS + 1];
    Message queue[NUM_LIGHTWEIGHTS];
    int queue_size;
} Light;

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

int listenOnPort(int listen_port) {
    struct sockaddr_in s_addr;
    int fd_socket = -1;
    
    fd_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd_socket < 0) {// si el socket se ha creado bien, se salta este if
        printf("ERROR: Error en la creación del socket high lamport.\n");// si no se ha podido crear, sale el error
        return -1;
    }

    int opt = 1;
    setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(listen_port);

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

int connectToPort(int port) {
    for (int attempt = 0; attempt < 50; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in addr;

        if (fd < 0) {
            continue;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((unsigned short) port);
        if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
            close(fd);
            continue;
        }
        if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            close(fd);
            continue;
        }
        if (fd >= 0) {
            return fd;
        }
    }
    return -1;
}

int parseMessage(char *line, char *type, int type_len, int *id, int *timestamp) {
    char *rest = NULL;
    char *temp;

    if (line == NULL) {
        return -1;
    }

    temp = strtok_r(line, "&", &rest);
    if (temp == NULL) {
        return -1;
    }
    snprintf(type, type_len, "%s", temp);
    *id = 0;
    *timestamp = 0;

    if (strcmp(type, "S") == 0 || strcmp(type, "D") == 0) {
        return 0;
    }

    temp = strtok_r(NULL, "&", &rest);
    if (temp == NULL) {
        return -1;
    }
    *id = atoi(temp);

    temp = strtok_r(NULL, "&", &rest);
    if (temp == NULL) {
        return -1;
    }
    *timestamp = atoi(temp);
    return 0;
}

void removeRequest(Light *light, int id) {
    int count = 0;

    for (int i = 0; i < light->queue_size; i++) {
        if (light->queue[i].id != id) {
            light->queue[count++] = light->queue[i];
        }
    }
    light->queue_size = count;
}

void enqueue(Light *light, int timestamp, int id) {
    removeRequest(light, id);
    if (light->queue_size >= NUM_LIGHTWEIGHTS) {
        return;
    }

    int aux = light->queue_size;
    for (int i = 0; i < light->queue_size; i++) {
        if (timestamp < light->queue[i].timestamp || (timestamp == light->queue[i].timestamp && id < light->queue[i].id)) {
            aux = i;
            break;
        }
    }
    for (int i = light->queue_size; i > aux; i--) {
        light->queue[i] = light->queue[i - 1];
    }
    light->queue[aux].timestamp = timestamp;
    light->queue[aux].id = id;
    light->queue[aux].valid = 1;
    light->queue_size++;
}

int everyLightReplied(Light *light) {
    for (int id = 1; id <= NUM_LIGHTWEIGHTS; id++) {
        if (id != light->id && !light->reply_from[id]) {
            return 0;
        }
    }
    return 1;
}

int broadcast(Light *light, char *type, int timestamp) {
    for (int id = 1; id <= NUM_LIGHTWEIGHTS; id++) {
        if (id != light->id && light->light_fd[id] > 0) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s&%d&%d\n", type, light->id, timestamp);
            write(light->light_fd[id], buffer, strlen(buffer));
        }
    }
    return 0;
}

void serviceIncoming(Light *light) {
    int first = 1;

    while (1) {
        fd_set set;
        int max_fd = -1;

        FD_ZERO(&set);
        for (int id = 1; id <= NUM_LIGHTWEIGHTS; id++) {
            int fd = light->light_fd[id];
            if (id != light->id && fd >= 0) {
                FD_SET(fd, &set);
                if (fd > max_fd) {
                    max_fd = fd;
                }
            }
        }
        if (max_fd < 0) {
            return;
        }

        // hay que definir el tiempo máximo de espera de select para evitar un interbloqueo
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        if (first) {
            first = 0;
            tv.tv_usec = 100000;
        }

        if (select(max_fd + 1, &set, NULL, NULL, &tv) <= 0) {
            return;
        }

        for (int id = 1; id <= NUM_LIGHTWEIGHTS; id++) {
            int fd = light->light_fd[id];
            char *line;
            char type[16];
            int sender = 0, timestamp = 0;

            if (id != light->id && fd >= 0 && FD_ISSET(fd, &set)) {
                line = readUntil(fd, '\n');
                if (line == NULL) {
                    close(fd);
                    light->light_fd[id] = -1;
                } else if (parseMessage(line, type, sizeof(type), &sender, &timestamp) < 0 || sender < 1 || sender > NUM_LIGHTWEIGHTS) {
                    free(line);
                } else {
                    free(line);

                    if (strcmp(type, "R") == 0) {
                        if (timestamp > light->clock) {
                            light->clock = timestamp;
                        }
                        light->clock++;
                        enqueue(light, timestamp, sender);
                        char buf[64];
                        snprintf(buf, sizeof(buf), "Y&%d&%d\n", light->id, light->clock);
                        write(light->light_fd[sender], buf, strlen(buf));
                    } else if (strcmp(type, "Y") == 0) {
                        if (timestamp > light->clock) {
                            light->clock = timestamp;
                        }
                        light->clock++;
                        light->reply_from[sender] = 1;
                    } else if (strcmp(type, "L") == 0) {
                        if (timestamp > light->clock) {
                            light->clock = timestamp;
                        }
                        light->clock++;
                        removeRequest(light, sender);
                    }
                }
            }
        }
    }
}

void requestCS(Light *light) {
    int timestamp;

    light->clock++;
    timestamp = light->clock;

    for (int id = 1; id <= NUM_LIGHTWEIGHTS; id++) {
        light->reply_from[id] = 0;
    }
    enqueue(light, timestamp, light->id);
    broadcast(light, "R", timestamp);

    while (light->queue[0].id != light->id || !everyLightReplied(light)) {
        serviceIncoming(light);
    }
}

void waitHeavyWeight(Light *light) {
    char *line = readUntil(light->heavy_fd, '\n');
    char type[16];
    int id = 0, timestamp = 0;

    if (parseMessage(line, type, sizeof(type), &id, &timestamp) < 0 || strcmp(type, "S") != 0) {
        fprintf(stdout, "Lamport light%d esperaba S del heavy\n", light->id);
        free(line);
        exit(1);
    }
    free(line);
}

int connectOtherLights(Light *light) {
    for (int id = 1; id <= NUM_LIGHTWEIGHTS; id++) {
        light->light_fd[id] = -1;
    }

    for (int count = light->id + 1; count <= NUM_LIGHTWEIGHTS; count++) {
        int fd = connectToPort(LAMPORT_LIGHT_PORT + count);
        if (fd < 0) {
            return -1;
        }
        char hello[32];
        snprintf(hello, sizeof(hello), "%d\n", light->id);
        write(fd, hello, strlen(hello));
        light->light_fd[count] = fd;
    }

    for (int n = 0; n < light->id - 1; n++) {
        int fd = accept(light->listen_fd, NULL, NULL);
        if (fd < 0) {
            return -1;
        }

        char *line;
        line = readUntil(fd, '\n');
        int id = 0;
        if (line == NULL || sscanf(line, "%d", &id) != 1 || id < 1 || id > NUM_LIGHTWEIGHTS) {
            free(line);
            close(fd);
            return -1;
        }
        free(line);
        light->light_fd[id] = fd;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    Light light;
    int id;
    char aux[32];

    id = atoi(argv[1]);

    memset(&light, 0, sizeof(light));

    light.id = id;
    snprintf(light.my_id, sizeof(light.my_id), "A%d", id);

    light.heavy_fd = connectToPort(LAMPORT_CONNECT_HEAVY_PORT);
    if (light.heavy_fd < 0) {
        fprintf(stdout, "%s no se ha podido conectar al heavy\n", light.my_id);
        return 1;
    }
    snprintf(aux, sizeof(aux), "%d\n", light.id);
    write(light.heavy_fd, aux, strlen(aux));

    light.listen_fd = listenOnPort(LAMPORT_LIGHT_PORT + light.id);
    if (light.listen_fd < 0) {
        perror("listen");
        return 1;
    }

    if (connectOtherLights(&light) < 0) {
        fprintf(stdout, "%s no se ha podido conectar a los otros lights\n", light.my_id);
        return 1;
    }
    close(light.listen_fd);
    light.listen_fd = -1;

    while (1) {
        waitHeavyWeight(&light);
        // difusión de la petición de sección crítica
        requestCS(&light);
        for (int i = 0; i < 10; i++) {
            usleep(150000);
            printf("I'm lightweight process %s\n", light.my_id);
            serviceIncoming(&light);
        }
        removeRequest(&light, light.id);
        light.clock++;
        // difusión de la liberación de la sección crítica
        broadcast(&light, "L", light.clock);
        write(light.heavy_fd, "D\n", strlen("D\n"));
    }
}
