//
// Created by Gael on 21/11/2026.
//

#include "transacciones.h"

int leerSiguienteOperacion(char **linea, char *destino) {
    char *p = *linea;
    int i = 0, dentroParentesis = 0;

    // Copia la siguiente operacion de la linea
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') {
        p++;
    }
    if (*p == '\0') {
        *linea = p;
        destino[0] = '\0';
        return 0;
    }

    while (*p != '\0') {
        if (*p == '(') {
            dentroParentesis = 1;
        } else if (*p == ')') {
            dentroParentesis = 0;
        } else if (*p == ',' && dentroParentesis == 0) {
            break;
        }
        destino[i] = *p;
        i++;
        p++;
    }

    destino[i] = '\0';
    *linea = p;
    return 1;
}

int leerLinea(char *linea, Transaccion *transaccion) {
    char *resto = linea;
    char texto[64];
    int tieneEscritura = 0;

    memset(transaccion, 0, sizeof(*transaccion));

    while (leerSiguienteOperacion(&resto, texto)) {
        Operacion operacion;
        operacion.campo = 0;
        operacion.valor = -1;

        if (texto[0] == 'b' || texto[0] == 'B') {
            operacion.tipo = 0;
            if (texto[1] != '\0') {
                operacion.campo = texto[1] - '0';
            }
            transaccion->capa = operacion.campo;
        } else if (texto[0] == 'r' || texto[0] == 'R') {
            operacion.tipo = 1;
            sscanf(texto + 2, "%d", &operacion.campo);
        } else if (texto[0] == 'w' || texto[0] == 'W') {
            operacion.tipo = 2;
            sscanf(texto + 2, "%d,%d", &operacion.campo, &operacion.valor);
            tieneEscritura = 1;
        } else {
            operacion.tipo = 3;
        }

        Operacion *nueva = realloc(transaccion->operacion, (transaccion->numOperaciones + 1) * sizeof(Operacion));
        transaccion->operacion = nueva;
        transaccion->operacion[transaccion->numOperaciones] = operacion;
        transaccion->numOperaciones++;
    }

    if (transaccion->numOperaciones == 0) {
        return 0;
    }
    if (tieneEscritura) {
        transaccion->soloLectura = 0;
    } else {
        transaccion->soloLectura = 1;
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
        Transaccion transaccion;
        if (leerLinea(linea, &transaccion) != 0) {
            Transaccion *nueva = realloc(lista->transaccion, (lista->numTransacciones + 1) * sizeof(Transaccion));
            lista->transaccion = nueva;
            lista->transaccion[lista->numTransacciones] = transaccion;
            lista->numTransacciones++;
        }
    }
    fclose(fichero);
    return 0;
}

void liberarTransacciones(ListaTransacciones *lista) {
    for (int i = 0; i < lista->numTransacciones; i++) {
        free(lista->transaccion[i].operacion);
    }
    free(lista->transaccion);
    lista->transaccion = NULL;
    lista->numTransacciones = 0;
}
