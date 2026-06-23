// 2 tuberías anónimas para simular la comunicación bidireccional
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>

#define MAXMSJ 100 //Tamaño max del mensaje

int main() {

    int fd_padre_hijo[2]; //Descriptores xra la tubería de comunicación unidireccional padre=>hijo
    int fd_hijo_padre[2]; //Descriptores xra la tubería de comunicación unidireccional hijo=>padre
    if( pipe(fd_padre_hijo) == -1 || pipe(fd_hijo_padre) == -1){
        perror("pipe");
        exit(1);
    }

    pid_t pid_hijo = fork();
    if(pid_hijo == -1){
        perror("fork");
        exit(1);
    }
    else if(pid_hijo == 0){ //CODIGO PROCESO HIJO

        close(fd_padre_hijo[1]); //Se cierra el extremo de escritura de la tubería padre_hijo => Proceso hijo no escribe en esta tubería
        close(fd_hijo_padre[0]); //Se cierra el extremo de lectura de la tubería hijo_padre => Proceso hijo no lee esta tubería
        int numMensajes = 0;

        while (1) { //Bucle del proceso padre 

            //El proceso hijo leerá de la tubería padre_hijo
            char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido desde la tubería padre_hijo
            read(fd_padre_hijo[0], mensaje, sizeof(mensaje)); //=> Bloqueante
            numMensajes++; //"Sale" del read => se ha leído un nuevo mensaje

            //Convertirá el mensaje a mayúsculas
            for (int i = 0; mensaje[i] != '\0'; i++) {
                mensaje[i] = toupper((char)mensaje[i]);
            }
            //Lo mostrará por la salida estándar.
            printf("Mensaje en mayusculas: %s\n", mensaje);

            //Esperará durante 1 segundo
            sleep(1);

            //Trascurrida la espera:
            //Enviará el carácter ‘n’ al proceso padre por la tubería hijo_padre para indicar que esta listo para recibir un nuevo mensaje
            //O enviará el carácter ‘q’ al padre para indicarle que debe finalizar cuando se hayan recibido 10 mensajes
            char caracter = 'n';
            if(numMensajes == 10) caracter = 'q';
            //Se enviará el mensaje al proceso padre escribiendo en la tubería hijo_padre.
            write(fd_hijo_padre[1], &caracter, sizeof(caracter)); 

            if(numMensajes == 10) break; //Cuando se llega a 10 mensajes, el proceso hijo, una vez ha mandado el caracter 'q' al padre, finaliza su ejecución
        }

        close(fd_padre_hijo[0]);
        close(fd_hijo_padre[1]); //Se cierra el extremo de escritura de la tubería hijo_padre
        exit(0);
    }
    else { //CODIGO PROCESO PADRE

        close(fd_padre_hijo[0]); //Se cierra el extremo de lectura de la tubería padre_hijo => proceso padre no lee esta tubería
        close(fd_hijo_padre[1]); //Se cierra el extremo de escritura de la tubería hijo_padre => proceso padre no escribe en esta tubería

        while(1) {
            //El proceso padre leerá de la entrada estándar un mensaje (una cadena)
            printf("Introduzca un mensaje: \n");
            printf("> "); 
            char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en la entrada éstandar
            if(fgets(mensaje, MAXMSJ, stdin) == NULL) { //Si (EOF/CTRL+D) o error
                exit(1); //Se termina la ejecución del padre
            }

            //Enviará el mensaje al proceso hijo escribiendo en la tubería padre_hijo.
            write(fd_padre_hijo[1], mensaje, sizeof(mensaje)); 

            //El proceso hijo leerá de la tubería hijo_padre
            char caracter; 
            read(fd_hijo_padre[0], &caracter, sizeof(caracter)); //=> Bloqueante

            if(caracter == 'q') break;;
        }

        close(fd_padre_hijo[1]); 
        close(fd_hijo_padre[0]);
        exit(0);
    }
}

//No cerrar los extremos de las tuberías dentro del bucle!!!!! Eso romperá la comunicación 