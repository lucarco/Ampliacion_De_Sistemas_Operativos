//TUBERÍA SIN NOMBRE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>

int main (int argc, char *argv[]) {

    //./ejercicio1 ls-l wc-l => argc = 5
    if(argc != 5){
        perror("Invalid arguments");
        exit(1);
    }

    //Creación de la tubería
    int read_fd, write_fd; 
    int pipe_fds[2];
    if(pipe(pipe_fds) == -1){
        perror("pipe");
        exit(1);
    }
    read_fd = pipe_fds[0]; //Extremo de lectura (donde se pueden leer datos)
    write_fd = pipe_fds[1]; //Extremo de escritura (donde se pueden escribir datos)

    //Creación del proceso hijo:
    pid_t pid_hijo = fork();

    //Los datos a trasladar mediante una tubería desde el proceso padre al proceso hijo; 
    //Se leen desde la entrada entrada estándar, pues son <comando2 argumento2>

    if(pid_hijo == -1){
        perror("fork");
        exit(1);
    }
    else if(pid_hijo == 0) { //Codigo del proceso hijo:  se ejecutará “comando2 argumento2”, redireccionandose previamente en este caso la entrada estándar al extremo de lectura del pipe.
        //1º se cierra el descriptor de escritura
        close(pipe_fds[1]);
        //2º se redirecciona la entrada estándar al extremo de escritura del pipe
        dup2(pipe_fds[0], STDIN_FILENO); 
        //3º Se ejecuta "comando2 (argv[3]) argumento2 (argv[4])"
        // Preparar los argumentos para execvp
        char *args[] = {argv[3], argv[4], NULL};
        if (execvp(argv[3], args) == -1) {
            perror("execvp");
            exit(1);
        }
    }
    else { //Codigo del proceso padre:  se ejecutará “comando1 argumento1”, redireccionando previamente la salida estándar al extremo de escritura del pipe.
        //1º se cierra el descriptor de lectura
        close(pipe_fds[0]);
        //2º se redirecciona la salida estándar al extremo de escritura del pipe
        dup2(pipe_fds[1], STDOUT_FILENO); 
        // Preparar los argumentos para execvp
        char *args[] = {argv[1], argv[2], NULL};
        //3º Se ejecuta "comando1 (argv[1]) argumento1 (argv[2])"
        if (execvp(argv[1], args) == -1) {
            perror("execvp");
            exit(1);
        }
    }
}


/*
Con el comando ./ejercicio1 ls -l wc -l:

El proceso padre ejecuta ls -l:

Este comando lista archivos en formato largo
Su salida normalmente iría a la pantalla
Pero como redirigimos STDOUT a la tubería, la salida va a la tubería


El proceso hijo ejecuta wc -l:

Este comando cuenta líneas
Su entrada normalmente vendría del teclado
Pero como redirigimos STDIN para que lea de la tubería, recibe la lista de archivos que generó ls -l
Por tanto, cuenta las líneas en esa lista de archivos
*/

/* A tener en cuenta: 
int execvp(const char *file, char *const argv[]);
Donde:

file: Es el nombre del programa que quieres ejecutar
argv: Es un array de strings (terminado en NULL) que contiene el nombre del programa y sus argumentos
*/