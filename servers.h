#ifndef SERVERS_H
#define SERVERS_H

/*
server-server: D&posicion_array&nuevo_valor, A&x&x (ack)
cliente-server: W&posicion_array&nuevo_valor, L&posicion_array&X, A&x&x (ack)
server-cliente: R&valor_de_la_posicion&x, A&x&x (ack)

*/

#define server_portA=5000
#define server_portB=5100
#define server_portC=5200
#define port_Client=5300

void ejecutarCore(int id);

#endif