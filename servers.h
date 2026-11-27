#ifndef SERVERS_H
#define SERVERS_H

/*
 * Cliente -> nodo:  L&indice&0 (READ)   W&indice&valor (WRITE)   A&0&0 (ACK)
 * Nodo -> cliente:  R&valor&0 (RESPONSE)   A&0&0 (ACK)
 * Core -> core:     D&Ta0&Ta1&Ta2&indice&valor (1 vez para actualizar 1 celda en otros nodos)   respuesta A (ACK) o S (ERROR)
 * Hacia B y C:      D&indice&valor (10 veces, celdas 0 a 9) y despues V&0&version
 *
 * De nodo hacia monitor      {"nodo":"A1","version":3,"pos":49,"valor":53}                         (Informar python de actualizacion)
 * De monitor hacia navegador {"tipo":"actualizacion","nodo":"A1","version":3,"pos":49,"valor":53}  (Informar navegador en un websocket)
 *
 * A2 envia su copia a B1 y A3 a B2 cada 10 escrituras.
 * B2 envia su copia a C1 y a C2 cada 10 segundos.
 */

void ejecutarCore(int id);
void ejecutarLayerB(int id);
void ejecutarLayerC(int id);

#endif
