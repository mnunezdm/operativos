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
#include <ctype.h>
#include "filtrar.h"
#include <errno.h>
#include <dlfcn.h>

/* ---------------- PROTOTIPOS ----------------- */
/* Esta funcion monta el filtro indicado y busca el simbolo "tratar"
   que debe contener, y aplica dicha funcion "tratar()" para filtrar
   toda la informacion que le llega por su entrada estandar antes
   de enviarla hacia su salida estandar. */
extern void filtrar_con_filtro(char* nombre_filtro);

/* Esta funcion lanza todos los procesos necesarios para ejecutar los filtros.
   Dichos procesos tendran que tener redirigida su entrada y su salida. */
void preparar_filtros(void);

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

void manejar_alarma(void);


/* ---------------- IMPLEMENTACIONES ----------------- */
char** filtros;   /* Lista de nombres de los filtros a aplicar */
int    n_filtros; /* Tama~no de dicha lista */
pid_t* pids;      /* Lista de los PIDs de los procesos que ejecutan los filtros */



/* Funcion principal */
int main(int argc, char* argv[]) {
    /* Chequeo de argumentos */
    if (argc < 2){
        /* Invocacion sin argumentos  o con un numero de argumentos insuficiente */
        fprintf(stderr, "Uso: %s directorio [filtro...]\n", argv[0]);
        exit(1);
    }

    filtros = &(argv[2]);                             /* Lista de filtros a aplicar */
    n_filtros = argc-2;                               /* Numero de filtros a usar */
    pids = (pid_t*)malloc(sizeof(pid_t)*n_filtros);   /* Lista de pids */

    preparar_alarma();

    if (n_filtros > 0) {
        preparar_filtros();
    }

    recorrer_directorio(argv[1]);
		
    esperar_terminacion();

    return 0;
}

extern void preparar_alarma(void){
	char* entorno;
	int valor;
	struct sigaction act;
	if ((entorno = getenv("FILTRAR_TIMEOUT")) != NULL){
		if(!isdigit(entorno[0])){
			fprintf(stderr,"Error FILTRAR_TIMEOUT no es entero positivo: '%s'\n", entorno);
			return;
		}
		valor = strtol(entorno, NULL, 10);
		sigemptyset(&act.sa_mask);
		act.sa_handler=(void*)manejar_alarma;
		act.sa_flags = SA_RESTART;
		sigaction(SIGALRM,&act,NULL); 
		alarm(valor);
		fprintf(stderr, "AVISO: La alarma vencera tras %d segundos!\n", valor);
	}	
}

void manejar_alarma(void) {
	int i;
	fprintf(stderr,"AVISO: La alarma ha saltado!\n");
	if (n_filtros > 0) {
		for (i = 0; i < n_filtros; i++) {
			if (!kill(pids[i], 0)) {
				kill(pids[i],SIGKILL);
			}
		}
	}
	esperar_terminacion();	
	exit(1);
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
    if ((ent=readdir(dir)) == NULL) {
        fprintf(stderr, "Error al leer el directorio '%s'\n",nombre_dir);
        exit(1);
    }
		rewinddir(dir);
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
        if (S_ISDIR(estado.st_mode) || S_ISLNK(estado.st_mode))
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
    /* Cerrar. */
		closedir(dir);
		close(1);
    /* IMPORTANTE:
     * Para que los lectores del pipe puedan terminar
     * no deben quedar escritores al otro extremo. */
    // IMPORTANTE
}

void preparar_filtros(void) {
    int i;
    int fd[n_filtros][2];
    char* ext;
		for (i = n_filtros-1; i >= 0; i--) {
      /* Tuberia hacia el hijo (que es el proceso que filtra). */
			//fprintf(stderr, "generando hijo %d, filtro %s\n", i, filtros[i]);
			if (pipe(fd[i])){
    		fprintf(stderr,"Error al crear el pipe\n");
      	exit(1);
			}
			/* Lanzar nuevo proceso */  
			switch(pids[i]=fork()){
      	case -1:
        	/* Error. Mostrar y terminar. */
          fprintf(stderr,"Error al crear proceso %d\n",pids[0]);
          exit(1);
          break;
        case  0:
          /* Hijo: Redireccion y Ejecuta el filtro. */
          close(0);
          dup(fd[i][0]);
					close(fd[i][0]);
					close(fd[i][1]);
          ext=strrchr(filtros[i],'.');
          /* El nombre termina en .so
          montamos la libreria estandar */
          if (ext && !strcmp(ext,".so")){
          	filtrar_con_filtro(filtros[i]);
          }
          /* Mandato estandar */
          else {
						execlp(filtros[i],filtros[i], NULL);
    				fprintf(stderr, "Error al ejecutar el mandato '%s'\n", filtros[i]);
    				exit(1);
          }
          break;
				default:
          /* Padre: Redireccion */
					close(1);
					dup(fd[i][1]);
          close(fd[i][0]);
          close(fd[i][1]);
					break;
        }
    }
}

void filtrar_con_filtro(char* nombre_filtro) {
	void * handler;
	int (*tratar)(char*, char*, int);
	int tratados, leidos=0;
	char buff_in[1024], buff_out[1024], libreria[128];
	getcwd(libreria, sizeof(buff_in));
	strcat(libreria, "/");
	strcat(libreria, nombre_filtro);
	handler = dlopen(libreria,RTLD_LAZY);
	if (!handler){
		fprintf(stderr,"Error al abrir la biblioteca '%s'\n",libreria);
		exit(1);
	}
	tratar = dlsym(handler, "tratar");
	if (dlerror() != NULL){
		fprintf(stderr,"Error al buscar el simbolo '%s' en '%s'\n","tratar",nombre_filtro);
		exit(1);
	}
	while ((leidos = read(0, buff_in, 1024)) > 0) {
		tratados = tratar(buff_in, buff_out, leidos);
		write(1, buff_out, tratados);
	}
	if (dlclose(handler) < 0) {
		exit(1);
	}
}

void imprimir_estado(char* filtro, int status) {
  /* Imprimimos el nombre del filtro y su estado de terminacion */
 	if(WIFEXITED(status)){
		fprintf(stderr, "%s: %d\n", filtro, WEXITSTATUS(status));
  } else {
  	fprintf(stderr,"%s: senal %d\n",filtro,WTERMSIG(status));
	}
}

void esperar_terminacion(void){
	int p, status;
  for(p=0;p<n_filtros;p++) {
		/* Espera al proceso pids[p] */
		if (waitpid(-1, &status, 0)<0) {
			fprintf(stderr,"Error al esperar proceso %d\n", pids[p]);
			exit(1);
		}
		/* Muestra su estado. */
		imprimir_estado(filtros[p], status);
	}
}
