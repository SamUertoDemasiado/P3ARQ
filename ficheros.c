//
// Created by Gael on 21/11/2026.
//

#include "ficheros.h"

int siguienteToken(char **linea, char *dest) {
    char *s = *linea;
    int paren = 0, count = 0;

    while (isspace((unsigned char)*s) || *s == ',') {
        s++;
    }

    if (*s == '\0') {
        *linea = s;
        dest[0] = '\0';
        return 0;
    }

    while (*s != '\0' && !(*s == ',' && paren == 0)) {
        if (*s == '(') {
            paren++;
        }
        else if (*s == ')') {
            paren--;
        }
        dest[count++] = *s++;
    }

    dest[count] = '\0';
    *linea = s;
    return 1;
}

int parsearLinea(char *linea, Transaccion *tx) {
    char *lineaAux = linea;
    char token[64];
    int tieneEscritura = 0;

    memset(tx, 0, sizeof(*tx));

    while (siguienteToken(&lineaAux, token)) {
        Operacion operacion;
        operacion.campo = 0;
        operacion.valor = -1;

        if (token[0] == 'b' || token[0] == 'B') {
            operacion.tipo = 0;
            if (token[1] != '\0') {
                operacion.campo = token[1] - '0';
            }
            tx->capa = operacion.campo;
        } else if (token[0] == 'r' || token[0] == 'R') {
            operacion.tipo = 1;
            sscanf(token + 2, "%d", &operacion.campo);
        } else if (token[0] == 'w' || token[0] == 'W') {
            operacion.tipo = 2;
            sscanf(token + 2, "%d,%d", &operacion.campo, &operacion.valor);
            tieneEscritura = 1;
        } else {
            operacion.tipo = 3;
        }

        Operacion *nueva = realloc(tx->operacion, (tx->numOperaciones + 1) * sizeof(Operacion));
        if (nueva == NULL) {
            return 0;
        }
        tx->operacion = nueva;
        tx->operacion[tx->numOperaciones] = operacion;
        tx->numOperaciones++;
    }

    if (tx->numOperaciones == 0) {
        return 0;
    }
    if(tieneEscritura == 1){
        tx->soloLectura = 0;
    }
    else{
        tx->soloLectura = 1;
    }
    return 1;
}

int leerTransacciones(char *ruta, ListaTransacciones *lista) {
    FILE *fichero;
    char linea[512];

    lista->transaccion = NULL;
    lista->numTransacciones = 0;

    fichero = fopen(ruta, "r");
    if (fichero == NULL) {
        perror(ruta);
        return -1;
    }

    while (fgets(linea, sizeof(linea), fichero) != NULL) {
        Transaccion tx;
        int resultado = parsearLinea(linea, &tx);
        if (resultado == 0) {
            continue;
        }
        Transaccion *nueva = realloc(lista->transaccion, (lista->numTransacciones + 1) * sizeof(Transaccion));
        lista->transaccion = nueva;
        lista->transaccion[lista->numTransacciones] = tx;
        lista->numTransacciones++;
    }

    fclose(fichero);
    return 0;
}

void liberarTransacciones(ListaTransacciones *lista) {
    for (int i = 0; i < lista->numTransacciones; i++) {
        free(lista->transaccion[i].operacion);
        lista->transaccion[i].operacion = NULL;
    }
    free(lista->transaccion);
    lista->transaccion = NULL;
    lista->numTransacciones = 0;
}
