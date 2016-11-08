#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include "filtrar.h"
#include <errno.h>

/* ---------------- PROTOTIPOS ----------------- */ 
/* Esta funcion monta el filtro indicado y busca el simbolo "tratar"
   que debe contener, y aplica dicha funcion "tratar()" para filtrar
   toda la informacion que le llega por su entrada estandar antes
   de enviarla hacia su salida estandar. */
extern void filtrar_con_filtro(char* nombre_filtro);

/* Esta funcion lanza todos los procesos necesarios para ejecutar los filtros.
   Dichos procesos tendran que tener redirigida su entrada y su salida. */
void preparar_filtros(int nfiltros, char* filtros[]);

/* Esta funcion recorrera el directorio pasado como argumento y por cada entrada
   que no sea un directorio o cuyo nombre comience por un punto '.' la lee y 
   la escribe por la salida estandar (que seria redirigida al primero de los 
   filtros, si existe). */
void recorrer_directorio(char* nombre_dir);

/* Esta funcion recorre los procesos arrancados para ejecutar los filtros, 
   esperando a su terminacion y recogiendo su estado de terminacion. */ 
void esperar_terminacion(void);

/* Desarrolle una funcion que permita controlar la temporizacion de la ejecucion
   de los filtros. */ 
extern void preparar_alarma(void);


/* ---------------- IMPLEMENTACIONES ----------------- */ 
char** filtros;   /* Lista de nombres de los filtros a aplicar */
int    n_filtros; /* Tama~no de dicha lista */
pid_t* pids;      /* Lista de los PIDs de los procesos que ejecutan los filtros */



/* Funcion principal */
int main(int argc, char* argv[])
{
	/* Chequeo de argumentos */
	if (argc < 2){
		/* Invocacion sin argumentos  o con un numero de argumentos insuficiente */
		fprintf(stderr, "Uso: %s directorio [filtro...]\n", argv[0]);
		exit(1);
	}

	filtros = &(argv[2]);                             /* Lista de filtros a aplicar */
	n_filtros = argc-2;                               /* Numero de filtros a usar */
	pids = (pid_t*)malloc(sizeof(pid_t)*n_filtros);   /* Lista de pids */

	//	preparar_alarma();
	
	if (n_filtros > 0) {
		preparar_filtros(n_filtros, filtros);
	}
	
	recorrer_directorio(argv[1]);

	//esperar_terminacion();

	return 0;
}


void recorrer_directorio(char* nombre_dir) {
	DIR* dir = NULL;
	struct dirent* ent;
	char fich[1024];
	char relativo[1024];
	char buff[4096];
	int fd;
	struct stat estado;
	/* Abrir el directorio */
	dir = opendir(nombre_dir);
	/* Tratamiento del error. */
	if (dir == NULL) {
		fprintf(stderr, "Error al abrir el directorio '%s'\n",nombre_dir);
		exit(1);
	}
	if(access(nombre_dir,X_OK) == -1){
		fprintf(stderr, "Error al leer el directorio '%s'\n",nombre_dir);
		exit(1);
	}	
	/* Recorremos las entradas del directorio */
	while((ent=readdir(dir))!=NULL){
		/* Nos saltamos las que comienzan por un punto "." */
		if(ent->d_name[0]=='.')
			continue;
		/* fich debe contener la ruta completa al fichero */
		getcwd(fich, sizeof(fich));
		strcat(fich, "/");
		strcpy(relativo, nombre_dir);
		strcat(relativo, "/");
		strcat(relativo, ent->d_name);
		strcat(fich, relativo);
		/* Nos saltamos las rutas que sean directorios. */
		if (stat(fich,&estado) != 0){
			fprintf(stderr, "AVISO: No se puede stat el fichero '%s'!\n", relativo);
			continue;
		}
		if (S_ISDIR(estado.st_mode))
			continue;
		/* Abrir el archivo. */
		fd = open(fich, O_RDONLY);
		/* Tratamiento del error. */
		if (fd < 0){
			fprintf(stderr, "AVISO: No se puede abrir el fichero '%s'!\n",relativo);
			continue;
		}
		/* Cuidado con escribir en un pipe sin lectores! */
		if(signal(SIGPIPE,SIG_IGN)==SIG_ERR){
			fprintf(stderr,"Error al emitir el fichero '%s'\n",relativo);
			close(fd);
			break;
		}
		/* Emitimos el contenido del archivo por la salida estandar. */
		while (write(1,buff,read(fd,buff,4096)) > 0)
			continue;
		/* Cerrar. */
		close(fd);
	}
	if (errno != 0) {
		fprintf(stderr, "Error al leer el directorio '%s'\n",nombre_dir);
		exit(1);
	}
	/* Cerrar. */
	closedir(dir);
	/* IMPORTANTE:
	 * Para que los lectores del pipe puedan terminar
	 * no deben quedar escritores al otro extremo. */
	// IMPORTANTE
}

void preparar_filtros(int nfiltros, char* filtros[]) {
	int p, i;
	int fd[2];
	char* ext;
	for (i = nfiltros-1; i >= 0; i--) {
		/* Tuberia hacia el hijo (que es el proceso que filtra). */
		p = pipe(fd);
		if (pipe(fd) < 0) {
			fprintf(stderr,"Error al crear el pipe\n");
			exit(1);
		}
		/* Lanzar nuevo proceso */
		switch(p=fork()){
			case -1:
				/* Error. Mostrar y terminar. */
				fprintf(stderr,"Error al crear proceso %d\n",p);
				exit(1);
				break;
			case  0:
				/* Hijo: Redireccion y Ejecuta el filtro. */
				close(0);
				dup(fd[0]);
				close(fd[0]);
				close(fd[1]);
				ext=strrchr(filtros[i],'.');
				/* El nombre termina en .so
				 montamos la libreria estandar */
				if (ext && !strcmp(ext,".so")){
					fprintf(stderr,"Error al ejecutar el filtro '%s'\n", filtros[i]);	
				}
				/* Mandato estandar */
				else {
					filtrar_con_filtro(filtros[i]);
				}
				break;
			default:
				/* Padre: Redireccion */
				close(1);
				dup(fd[1]);
				close(fd[0]);
				close(fd[1]);
				break;
		}
	}
}

void filtrar_con_filtro(char* nombre_filtro) {
	execlp(nombre_filtro, nombre_filtro, NULL);
	fprintf(stderr, "Error al ejecutar el mandato '%s'\n", nombre_filtro);
	exit(1);
}

void imprimir_estado(char* filtro, int status)
{
	/* Imprimimos el nombre del filtro y su estado de terminacion */
	if(WIFEXITED(status))
		printf(stderr,"%s: %d\n",filtro,WEXITSTATUS(status));
	else
		printf(stderr,"%s: senal %d\n",filtro,WTERMSIG(status));
}


void esperar_terminacion(void){
	int p;
  for(p=0;p<n_filtros;p++) {
	/* Espera al proceso pids[p] */

	/* Muestra su estado. */
		
  }
}
