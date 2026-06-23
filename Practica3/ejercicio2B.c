#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <sys/types.h> //=> mkfifo 
#include <sys/stat.h>

int main () {

    char mensaje[] = "breiko breiko!\n";

    /*Creación de mi tubería FIFO $HOME\tuberia1
    if ( mkfifo("./~tuberia1", 0666) == -1 ){
        perror("mkfifo");
        exit(1);
    } 
    */

    // Abrir la FIFO en modo escritura
    int fd = open("/home/usuarioso/tuberia1", O_WRONLY); 
    if (fd == -1) {
       perror("Error al abrir la FIFO");
       exit(1);
    }

    write(fd, mensaje, strlen(mensaje));
    close(fd);
    return 0;
}