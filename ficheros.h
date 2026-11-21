//
// Created by Gael on 21/11/2026.
//

#ifndef CLIENTE_H
#define CLIENTE_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int tipo; // 0-begin 1-read 2-write 3-commit
    int campo;
    int valor;
} Operacion;

typedef struct {
    int capa;
    int soloLectura;
    Operacion *operacion;
    int numOperaciones;
} Transaccion;

typedef struct {
    Transaccion *transaccion;
    int numTransacciones;
} ListaTransacciones;

int leerTransacciones(char *ruta, ListaTransacciones *lista);

void liberarTransacciones(ListaTransacciones *lista);

#endif
