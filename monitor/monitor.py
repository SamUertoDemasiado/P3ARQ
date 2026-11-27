'''
'' Created by Gael on 23/11/2026.
'''

import base64
import hashlib
import json
import os
import socket
import threading

NODOS = ["A1", "A2", "A3", "B1", "B2", "C1", "C2"]
navegadores = []
estado = {}

for nombre in NODOS:
    estado[nombre] = {"version": None, "pos": None, "valor": None}


def aplicar_linea(texto):
    mensaje = json.loads(texto)
    nodo = mensaje["nodo"]
    estado[nodo]["version"] = mensaje["version"]
    estado[nodo]["pos"] = mensaje["pos"]
    estado[nodo]["valor"] = mensaje["valor"]
    return {
        "tipo": "actualizacion",
        "nodo": nodo,
        "version": mensaje["version"],
        "pos": mensaje["pos"],
        "valor": mensaje["valor"],
    }


def enviar_texto(sock, texto):
    datos = texto.encode("utf-8")
    sock.sendall(bytes([0x81, len(datos)]) + datos)


def difundir(aviso):
    texto = json.dumps(aviso)
    for sock in navegadores:
        enviar_texto(sock, texto)


def atender_nodo(conn):
    buf = b""
    while True:
        bloque = conn.recv(4096)
        if not bloque:
            break
        buf += bloque
        while b"\n" in buf:
            linea, buf = buf.split(b"\n", 1)
            aviso = aplicar_linea(linea.decode("utf-8").strip())
            print("%s version %s" % (aviso["nodo"], aviso["version"]), flush=True)
            difundir(aviso)
    conn.close()


def servidor_nodos():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 8090))
    srv.listen(8)
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=atender_nodo, args=(conn,), daemon=True).start()


def aceptar_websocket(conn, clave):
    accept = base64.b64encode(hashlib.sha1((clave + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()).decode("ascii")
    respuesta = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n" % accept
    )
    conn.sendall(respuesta.encode("ascii"))
    navegadores.append(conn)
    for nombre in NODOS:
        ficha = estado[nombre]
        enviar_texto(conn, json.dumps({
            "tipo": "actualizacion",
            "nodo": nombre,
            "version": ficha["version"],
            "pos": ficha["pos"],
            "valor": ficha["valor"],
        }))


def servir_pagina(conn):
    ruta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "index.html")
    fichero = open(ruta, "rb")
    cuerpo = fichero.read()
    fichero.close()
    cabecera = (
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n" % len(cuerpo)
    )
    conn.sendall(cabecera.encode("ascii") + cuerpo)
    conn.close()


def atender_web(conn):
    buf = b""
    while b"\r\n\r\n" not in buf:
        buf += conn.recv(1024)
    texto = buf.decode("latin1")
    if "upgrade: websocket" in texto.lower():
        for linea in texto.split("\r\n"):
            if linea.lower().startswith("sec-websocket-key:"):
                aceptar_websocket(conn, linea.split(":", 1)[1].strip())
                return
    servir_pagina(conn)


def servidor_web():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", 8080))
    srv.listen(8)
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=atender_web, args=(conn,), daemon=True).start()


def main():
    threading.Thread(target=servidor_nodos, daemon=True).start()
    print("Pagina: http://127.0.0.1:%d" % 8080, flush=True)
    print("Los nodos avisan en %s:%d" % ("127.0.0.1", 8090), flush=True)
    servidor_web()


if __name__ == "__main__":
    main()
