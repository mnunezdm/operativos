#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>   
#include <errno.h>   
#include "../message.h"

int leer_puerto_de_fichero();

int abrir_socket_udp();

int solicitar_finalizacion_servidor(int sock_udp,
                                    int puerto_udp_servidor);

int solicitar_puerto_transmision(int sock_udp,
                                 int puerto_udp_servidor,
                                 char *fichero,
                                 UDP_Msg *respuesta_p);

void construir_peticion(UDP_Msg *mensaje_p,
                        char *fichero);

int abrir_conexion_tcp_con_servidor(int puerto_servidor);

void recibir_fichero(int sock,
                     int fich);

int main(int argc, char *argv[]) {
    int sock_udp; /* Socket (de tipo UDP) de trasmision de ordenes */
    UDP_Msg respuesta;/* Contestacion del servidor */
    int puerto_udp_servidor; /* Puerto UDP del servidor (proviene de un parametro de entrada) */
    int puerto_tcp_servidor; /* Puerto TCP del servidor (transmitido en el mensaje de solicitud) */
    int socket_datos; /* Socket (TCP) pata trasmision de datos */
    int fich; /* Descriptor del fichero a trasmitir */

    if (argc > 1 && !strcmp(argv[1], "--help")) {
        fprintf(stdout,
                "%s --help: Muestra la ayuda\n"
                        "%s [fichero_transmitir]\n", argv[0], argv[0]);
        return 0;
    }

    puerto_udp_servidor = leer_puerto_de_fichero();

    sock_udp = abrir_socket_udp();
    if (argc < 2)
        solicitar_finalizacion_servidor(sock_udp, puerto_udp_servidor);
    else if (solicitar_puerto_transmision(sock_udp, puerto_udp_servidor, argv[1], &respuesta)) {
        puerto_tcp_servidor = ntohs(respuesta.puerto);
        if ((fich = creat(respuesta.local, 0666))) {
            socket_datos = abrir_conexion_tcp_con_servidor(puerto_tcp_servidor);
            recibir_fichero(socket_datos, fich);
            close(fich);
            close(socket_datos);
        }
    }

    close(sock_udp);
    exit(0);
}


/* Abre el socket y le asigna la direccion UDP dinamicamente */
int abrir_socket_udp() {
    struct sockaddr_in dir_udp_cli; /* Direccion cliente */
    int sock_udp; /* Socket (de tipo UDP) de trasmision de ordenes */

    /* Creacion del socket UDP */
    fprintf(stdout, "CLIENTE:  Creacion del socket UDP: ");
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_udp < 0) {
        fprintf(stdout, "ERROR\n");
        exit(1);
    }
    fprintf(stdout, "OK\n");

    /* Asignacion de la direccion local (del cliente) */
    bzero((char *) &dir_udp_cli, sizeof(struct sockaddr_in)); /* Pone a 0
							    la estructura */
    dir_udp_cli.sin_family = AF_INET;
    dir_udp_cli.sin_addr.s_addr = inet_addr(HOST_CLIENTE);
    dir_udp_cli.sin_port = htons(0);
    fprintf(stdout, "CLIENTE:  Reserva del puerto cliente: ");
    if (bind(sock_udp,
             (struct sockaddr *) &dir_udp_cli,
             sizeof(struct sockaddr_in)) < 0) {
        fprintf(stdout, "ERROR\n");
        close(sock_udp);
        exit(1);
    }

    fprintf(stdout, "OK\n");

    return sock_udp;
}

/* El cliente manda un mensaje QUIT al servidor, y espera su finalizaci�n que
   vendr� corroborada por una respuesta OK. Toda esta comunicaci�n es por
   medio de los mensajes UDP. */
int solicitar_finalizacion_servidor(int sock_udp, int puerto_udp_servidor) {
    UDP_Msg mensaje;  /* Mensaje a trasmitir/recibir */
    struct sockaddr_in dir_udp_srv;  /* Direccion servidor */
    size_t tam_dir;  /* Tama~no de la direcion */

    /* Definicion de la direccion del servidor */
    bzero((char *) &dir_udp_srv, sizeof(struct sockaddr_in)); /* Pone a 0
							    la estructura */
    dir_udp_srv.sin_family = AF_INET;
    dir_udp_srv.sin_addr.s_addr = inet_addr(HOST_SERVIDOR);
    dir_udp_srv.sin_port = htons(puerto_udp_servidor);

    tam_dir = sizeof(struct sockaddr_in);

    bzero((void *) &mensaje, sizeof(mensaje));
    mensaje.op = htonl(QUIT);

    /* Envio de mensaje */
    fprintf(stdout, "CLIENTE:  Enviando QUIT(): ");

    if (sendto(sock_udp, (char *) &mensaje, sizeof(UDP_Msg), 0,
               (struct sockaddr *) &dir_udp_srv, tam_dir) < 0) {
        fprintf(stdout, "ERROR\n");
        close(sock_udp);
        exit(1);
    }
    fprintf(stdout, "OK\n");

    /* Lectura de la respuesta */
    fprintf(stdout, "CLIENTE:  Respuesta del servidor: ");
    if (recvfrom(sock_udp, (char *) &mensaje, sizeof(UDP_Msg), 0,
                 (struct sockaddr *) &dir_udp_srv, (socklen_t *) &tam_dir) < 0) {
        fprintf(stdout, "<<ERROR>>\n");
        close(sock_udp);
        exit(1);
    }

    fprintf(stdout, "%s\n",
            ntohl(mensaje.op) == OK ? "OK" : "ERROR");

    return ntohl(mensaje.op) == OK;
}

/* Solicita un puerto TCP para hacer la transmision del fichero solicitado.
   Esta operacion implica la construccion de un mensaje de solicitud y
   su transmision. Se debe considerar el caso de error (fichero no existente). */
int solicitar_puerto_transmision(int sock_udp, int puerto_udp_servidor, char *fichero, UDP_Msg *respuesta_p) {
    UDP_Msg mensaje;  /* Mensaje a trasmitir/recibir */
    struct sockaddr_in dir_udp_srv;  /* Direccion servidor */
    size_t tam_dir;  /* Tama~no de la direcion */

    /* Definicion de la direccion del servidor */
    bzero((char *) &dir_udp_srv, sizeof(struct sockaddr_in)); /* Pone a 0
							    la estructura */
    dir_udp_srv.sin_family = AF_INET;
    dir_udp_srv.sin_addr.s_addr = inet_addr(HOST_SERVIDOR);
    dir_udp_srv.sin_port = htons(puerto_udp_servidor);

    tam_dir = sizeof(struct sockaddr_in);

    construir_peticion(&mensaje, fichero);

    /* Envio de mensaje */
    fprintf(stdout, "CLIENTE:  Enviando REQUEST(%s,%s): ",
            mensaje.local,
            mensaje.remoto);

    if (sendto(sock_udp, (char *) &mensaje, sizeof(UDP_Msg), 0,
               (struct sockaddr *) &dir_udp_srv, tam_dir) < 0) {
        fprintf(stdout, "ERROR\n");
        close(sock_udp);
        exit(1);
    }
    fprintf(stdout, "OK\n");

    /* Lectura de la respuesta */
    fprintf(stdout, "CLIENTE:  Respuesta del servidor: ");
    if (recvfrom(sock_udp, (char *) respuesta_p, sizeof(UDP_Msg), 0,
                 (struct sockaddr *) &dir_udp_srv, (socklen_t *) &tam_dir) < 0) {
        fprintf(stdout, "<<ERROR>>\n");
        close(sock_udp);
        exit(1);
    }
    fprintf(stdout, "%s\n",
            ntohl(respuesta_p->op) == OK ? "OK" : "ERROR");

    /* Si la respuesta es afirmativa se procede a enviar/recibir
       el fichero */
    return (ntohl(respuesta_p->op) == OK);
}

/* Funcion que lee una operacion */
void construir_peticion(UDP_Msg *m, char *fichero) {
    m->op = htonl(REQUEST); /* El mensaje mandado es de tipo REQUEST */
    sprintf(m->local, "%s.local", fichero);  /* Fichero local: <fichero>.local */
    sprintf(m->remoto, "%s", fichero);  /* Fichero remoto, fichero original */
}

/* Funcion que abre un nuevo socket TCP para la transmision de datos */
int abrir_conexion_tcp_con_servidor(int puerto_servidor) {
    int sock_tcp; /* Socket (de tipo TCP) para transmision de datos */
    struct sockaddr_in dir_tcp_srv;  /* Direccion TCP servidor */

    /* Creacion del socket TCP */
    fprintf(stdout, "CLIENTE:  Creacion del socket TCP: ");
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_tcp < 0) {
        fprintf(stdout, "ERROR\n");
        exit(1);
    }
    fprintf(stdout, "OK\n");

    /* Nos conectamos con el servidor */
    bzero((char *) &dir_tcp_srv, sizeof(struct sockaddr_in)); /* Pone a 0
							    la estructura */
    dir_tcp_srv.sin_family = AF_INET;
    dir_tcp_srv.sin_addr.s_addr = inet_addr(HOST_SERVIDOR);
    dir_tcp_srv.sin_port = htons(puerto_servidor);
    fprintf(stdout, "CLIENTE:  Conexion al puerto servidor: ");
    if (connect(sock_tcp,
                (struct sockaddr *) &dir_tcp_srv,
                sizeof(struct sockaddr_in)) < 0) {
        fprintf(stdout, "ERROR %d\n", errno);
        close(sock_tcp);
        exit(1);
    }
    fprintf(stdout, "OK\n");

    return (sock_tcp);
}

/* Lee datos del socket y los manda al fichero */
void recibir_fichero(int sock, int fich) {
    char buff[512];
    int tam;

    while ((tam = (int) read(sock, (void *) buff, 512)) > 0) {
        write(fich, (void *) buff, tam);
    }
    close(fich);
    close(sock);
}

/* funcion para leer el puerto de servicio del ficherode configuracion */
int leer_puerto_de_fichero() {
    int puerto;
    int fd;
    if ((fd = open(FICHERO_PUERTO, O_RDONLY)) >= 0) {
        read(fd, &puerto, sizeof(int));
        close(fd);
        fprintf(stdout, "CLIENTE:  Puerto leido de fichero %s: OK\n", FICHERO_PUERTO);
    }
    return puerto;
}
