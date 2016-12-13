#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#define FICHERO_PUERTO       "./puerto_servidor"
#define HOST_SERVIDOR        "127.0.0.1"
#define HOST_CLIENTE         "127.0.0.1"

/* Operaciones disponibles */
#define REQUEST 1  /* remoto -> local */
#define OK      2  /* puede ser creado/leido el fichero remoto */
#define ERROR   3  /* NO puede ser creado/leido el fichero remoto */
#define QUIT    4  /* Cierra la conexión y finaliza el servidor */

typedef struct UDP_Msg_Struct
{
  int  op;           /* Operacion */
  char local[128];   /* Fichero local */
  char remoto[128];  /* Fichero remoto */
  int  puerto;       /* Puerto del servidor para la transmision del fichero via TCP */
} UDP_Msg;

#endif

