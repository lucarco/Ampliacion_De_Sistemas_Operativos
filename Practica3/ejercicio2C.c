#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <sys/types.h> //=> mkfifo 
#include <sys/stat.h>
#include <sys/select.h>

#define BUFF_LEN 100

int main () {

    /*
    if ( mkfifo("/home/usuarioso/tuberia2", 0666) == -1 ){
        perror("mkfifo");
        exit(1);
    } 
    */

    //Se abren las tuberías en modo lectura no bloqueante
    int fd1 = open("/home/usuarioso/tuberia1", O_RDONLY | O_NONBLOCK); 
    int fd2 = open("/home/usuarioso/tuberia2", O_RDONLY | O_NONBLOCK); 
    if (fd1 == -1 || fd2 == -1) {
       perror("Error al abrir la FIFO");
       exit(1);
    }

    int maxfd = (fd1 > fd2) ? fd1 : fd2; // Obtener el mayor fd

    //fd1 y fd2 pertenecerán al conjunto de descriptores a vigilar por si hay datos para leer:
    fd_set readfds;  //Conjunto de descriptores para lectura
    FD_ZERO(&readfds); //Se inicializa a vacio el conjunto
    FD_SET(fd1, &readfds);
    FD_SET(fd2, &readfds);

    //Se llama a select
    int ret = select(maxfd + 1, &readfds, NULL, NULL, NULL); //Timeout = null => espera indefinida
    if (ret == -1) {
       perror("Error en select");
       return 1;
    }

    char buffer[BUFF_LEN]; 

    if (FD_ISSET(fd1, &readfds)){
        if (read(fd1, buffer, sizeof(buffer)) == -1 ){
            perror("read");
            exit(1);
        }
        else printf("[Tuberia 1] Se ha escrito: %s\n", buffer);
    }
    else if (FD_ISSET(fd2, &readfds)) {
        if (read(fd2, buffer, sizeof(buffer)) == -1 ){
            perror("read");
            exit(1);
        }
        else printf("[Tuberia 2] Se ha escrito: %s\n", buffer);
    }

    close(fd1);
    close(fd2);
    return 0;
}