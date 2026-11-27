CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -D_DEFAULT_SOURCE
LDFLAGS = -pthread

CLIENTE = cliente
NODO    = P3ARQ

CLIENTE_SRCS = clienteP3.c transacciones.c comun.c
NODO_SRCS    = main.c servers.c comun.c

CLIENTE_OBJS = $(CLIENTE_SRCS:.c=.o)
NODO_OBJS    = $(NODO_SRCS:.c=.o)

TX ?= ejemplo.txt

.PHONY: all run clean

all: $(CLIENTE) $(NODO)

$(CLIENTE): $(CLIENTE_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENTE_OBJS) $(LDFLAGS)

$(NODO): $(NODO_OBJS)
	$(CC) $(CFLAGS) -o $@ $(NODO_OBJS) $(LDFLAGS)

clienteP3.o: clienteP3.c transacciones.h comun.h servers.h
transacciones.o: transacciones.c transacciones.h
comun.o: comun.c comun.h
main.o: main.c servers.h
servers.o: servers.c servers.h comun.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(CLIENTE)
	./$(CLIENTE) $(TX)

clean:
	rm -f $(CLIENTE_OBJS) $(NODO_OBJS) $(CLIENTE) $(NODO)
