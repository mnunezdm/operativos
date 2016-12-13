#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include "message.h"

#define BUF_SIZE 1024

// ---------------------------------------------------------------------------------------------------------------------
// DECLARACION DE METODOS Y VARIABLES GLOBALES

/** Escribe el puerto pasado como parametro en el fichero 'puerto_servidor' **/
void escribirPuerto(int puerto);

/** Abre un socket UDP en un puerto libre **/
int abrirSocketUDP();

/** Abre un socket TCP en un puerto libre **/
int abrirSocketTCP();

/** Bucle encargado de procesar las peticiones **/
int procesarPeticiones();

/** Variables globales **/
int socketTCP, socketUDP;
int puertoTCP;

// ---------------------------------------------------------------------------------------------------------------------
// IMPLEMENTACION DE METODOS

int main(int argc, char *argv[]) {
    // Creacion del Socket UDP
    if (abrirSocketUDP() < 0)
        return -1;

    // Creacion del Socket TCP
    if (abrirSocketTCP() < 0) {
        fprintf(stderr, "SERVIDOR: Puerto TCP reservado: ERROR\n");
        return -1;
    }
    fprintf(stderr, "SERVIDOR: Puerto TCP reservado: OK\n");

    // Inicio del bucle de procesar peticiones
    return procesarPeticiones();
}

void escribirPuerto(int puerto) {
    int fd;
    if ((fd = creat(FICHERO_PUERTO, 0660)) >= 0) {
        write(fd, &puerto, sizeof(int));
        close(fd);
        fprintf(stderr, "SERVIDOR: Puerto guardado en fichero %s: OK\n", FICHERO_PUERTO);
    }
}

int abrirSocketUDP() {
    struct sockaddr_in s_ain;
    socklen_t size = sizeof(s_ain);

    // Creamos el socket UDP
    fprintf(stderr, "SERVIDOR: Creacion del socket UDP: ");
    if ((socketUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "ERROR\n");
        return -1;
    }
    fprintf(stderr, "OK\n");

    // Creamos la direccion del socket UDP
    bzero((char *) &s_ain, sizeof(s_ain));
    s_ain.sin_family = AF_INET;
    s_ain.sin_addr.s_addr = inet_addr(HOST_SERVIDOR);
    s_ain.sin_port = htons(0);

    // Asignamos la direccion al socket UDP
    fprintf(stderr, "SERVIDOR: Asignacion del puerto servidor: ");
    if (bind(socketUDP, (struct sockaddr *) &s_ain, sizeof(s_ain)) < 0) {
        fprintf(stderr, "ERROR\n");
        close(socketUDP);
        return -1;
    }
    fprintf(stderr, "OK\n");

    // Obtenemos el puerto asignado dinamicamente y lo escribimos en el fichero
    bzero((char *) &s_ain, sizeof(struct sockaddr_in)); /* Pone a 0*/
    getsockname(socketUDP, (struct sockaddr *) &s_ain, &size);
    escribirPuerto(ntohs((uint16_t) (int) s_ain.sin_port) /* Numero de puerto asignado */);

    return 0;
}

int abrirSocketTCP() {
    //  Variables locales
    struct sockaddr_in s_ain;
    socklen_t size = sizeof(s_ain);

    // Creacion del socket **/
    fprintf(stderr, "SERVIDOR: Creacion del socket TCP: ");
    if ((socketTCP = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "ERROR\n");
        exit(1);
    }
    fprintf(stderr, "OK\n");

    // Creacion de la direccion a la que apunta el socket
    bzero((char *) &s_ain, sizeof(s_ain));
    s_ain.sin_family = AF_INET;
    s_ain.sin_addr.s_addr = inet_addr(HOST_SERVIDOR);
    s_ain.sin_port = htons(0);

    // Asignacion de la direccion al socket
    fprintf(stderr, "SERVIDOR: Asignacion del puerto servidor: ");
    if (bind(socketTCP, (struct sockaddr *) &s_ain, sizeof(s_ain)) < 0) {
        fprintf(stderr, "ERROR\n");
        close(socketTCP);
        return -1;
    }
    fprintf(stderr, "OK\n");

    // Obterner el puerto asignado automaticamente
    bzero((char *) &s_ain, sizeof(struct sockaddr_in)); /* Pone a 0*/
    getsockname(socketTCP, (struct sockaddr *) &s_ain, &size);
    puertoTCP = htons(s_ain.sin_port);

    // Aceptamos conexiones por el socket
    fprintf(stderr, "SERVIDOR: Aceptacion de peticiones: ");
    if (listen(socketTCP, 10) < 0) {
        fprintf(stderr, "ERROR");
        return -1;
    }
    fprintf(stderr, "OK\n");

    return 0;
}

int procesarPeticiones() {
    int mensaje_size, fichero, client_socket, stat = 0;
    struct sockaddr_in c_ain;
    socklen_t c_size;
    size_t len;
    ssize_t result;
    UDP_Msg mensaje;
    char buffer[BUF_SIZE];
    while (1) {
        // Limpiamos el contenido del struct mensaje
        bzero((char *) &c_ain, sizeof(struct sockaddr_in));
        c_size = sizeof(c_ain);
        bzero(&mensaje, sizeof(UDP_Msg));
        mensaje_size = sizeof(UDP_Msg);

        // Escuchamos en el socket UDP
        fprintf(stderr, "SERVIDOR: Esperando mensaje.\n");
        if (recvfrom(socketUDP, (char *) &mensaje, (size_t) mensaje_size, 0, (struct sockaddr *) &c_ain, &c_size) < 0) {
            fprintf(stderr, "SERVIDOR: Mensaje del cliente: ERROR\n");
            stat = -1;
            break;
        }
        fprintf(stderr, "SERVIDOR: Mensaje del cliente: OK\n");

        // Detectamos el tipo de mensaje
        if (ntohl((uint32_t) mensaje.op) == QUIT) {
            // Si tipo QUIT, enviamos un ACK y terminamos
            fprintf(stderr, "SERVIDOR: QUIT\n");
            mensaje.op = htonl(OK);
            fprintf(stderr,"SERVIDOR: Enviando del resultado [OK]: ");
            result = sendto(socketUDP, (char *) &mensaje, (size_t) mensaje_size, 0, (struct sockaddr *) &c_ain, c_size);
            fprintf(stderr, (result < 0)? "ERROR\n" : "OK\n");
            break;
        } else if (ntohl((uint32_t) mensaje.op) == REQUEST) {
            fprintf(stderr, "SERVIDOR: REQUEST(%s,%s)\n", mensaje.local, mensaje.remoto);
            // Si tipo REQUEST, intentamos abrir el fichero (solo lectura)
            fichero = open(mensaje.remoto, O_RDONLY);
            if (fichero < 0) {
                fprintf(stderr,"SERVIDOR: Enviando del resultado [ERROR]: ");
                mensaje.op = htonl(ERROR);
            } else {
                fprintf(stderr,"SERVIDOR: Enviando del resultado [OK]: ");
                mensaje.op = htonl(OK);
                mensaje.puerto = htons((uint16_t) puertoTCP);
            }
            // Enviamos el resultado del REQUEST al cliente correcta -> enviar puerto, incorrecta -> enviar error
            result = sendto(socketUDP, (char *) &mensaje, (size_t) mensaje_size, 0, (struct sockaddr *) &c_ain, c_size);
            fprintf(stderr, "%s\n", (result < 0)? "ERROR" : "OK");

            // Si el fichero se podia leer, iniciamos la conexion
            if (mensaje.op == htonl(OK)) {
                client_socket = accept(socketTCP, (struct sockaddr *) &c_ain, &c_size);
                fprintf(stderr,"SERVIDOR: Llegada de un mensaje: %s\n", (client_socket < 0)? "ERROR" : "OK");
                while ((len = (size_t) read(fichero, buffer, BUF_SIZE)) > 0)
                    write(client_socket, buffer, len);
                close(client_socket);
            }
            close(fichero);
        }
    }
    close(socketTCP);
    close(socketUDP);
    fprintf(stderr, "SERVIDOR: Finalizado\n");
    return stat;
}