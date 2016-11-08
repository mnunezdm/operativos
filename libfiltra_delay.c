#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "filtrar.h"

/* Este filtro deja pasar 100 caracteres por segundo. */
/* Devuelve el numero de caracteres que han pasado el filtro */
int tratar(char* buff_in, char* buff_out, int tam)
{
	usleep(tam*1000000.0/100);
	memcpy(buff_out, buff_in, tam);
	return tam;
}
