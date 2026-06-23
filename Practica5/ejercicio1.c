#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#define MAXMSJ 100 //Tamaño max del mensaje
#define MAILBOX_NAME "mailbox.txt"
#define EVENT_BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

int main() { 

    FILE* mailbox = fopen(MAILBOX_NAME, "a");  //Se crea mailbox.txt por si no existe 
    if (mailbox == NULL) {
        perror("No se pudo crear mailbox.txt");
        exit(1);
    }
    fclose(mailbox);

    int pausaSecs = 1; // Si PAUSASECS no esta definida por defecto se hara una pausa de 1 segundo

    char* varEntorno = getenv("PAUSASECS"); //Devuelve el valor asignado de la variable del entorno "PAUSASECS" (como cadena de caracteres)
    if(varEntorno != NULL) { 
        pausaSecs = atoi(varEntorno);
        if(pausaSecs == 0) {// Si da Error al convertir a entero 
            pausaSecs = 1; //Se asigna el valor por defecto
        }
    }

    //Se crea proceso hijo:
    pid_t pid_hijo = fork(); 

    if(pid_hijo == -1) { //Error al crear el proceso hijo
        perror("fork()");
        exit(1); 
    }
    else if(pid_hijo == 0) { //CÓDIGO DEL PROCESO HIJO

        //Se inicializa inotify para el hijo para detectar cuando el padre termine de escribir el mailbox
        int fd = inotify_init();
        if (fd == -1){
            perror("inotify_init");
            exit(1);
        }
        //Se añade el "watch" (monitor) del mailbox
        int wd = inotify_add_watch(fd, MAILBOX_NAME, IN_CLOSE_WRITE | IN_DELETE_SELF); //IN_CLOSE_WRITE: se cierra fichero abierto (para escritura) / IN_DELETE_SELF : se elimina fichero/directorio que se está monitorizando (para cuando EOF, mailbox se borra y esto notifica al proceso hijo)

        char buffer[EVENT_BUF_LEN]; //Donde se irá almacenando la cola de eventos detectados en el proceso hijo

        while(1) { //Bucle del proceso hijo:

           //SE ESPERA A QUE EL PADRE CIERRE EL MAILBOX TRAS ESCRIBIR EN ESTE (evento IN_CLOSE_WRITE)
           ssize_t length = read(fd, buffer, EVENT_BUF_LEN); 
           
            if (length == -1 && errno != EINTR) {
                perror("Error al leer eventos");
                break;
            }
            for (char *ptr = buffer; ptr < buffer + length; ) {
                struct inotify_event *event = (struct inotify_event *) ptr;

                if (event->mask & IN_CLOSE_WRITE) { //Se ha detectado en la cola de eventos el evento IN_CLOSE_WRITE
                    printf("Evento IN_CLOSE_WRITE detectado en el proceso hijo\n");

                    //SE LEEN LOS DATOS DISPONIBLES EN EL MAILBOX:
                    mailbox = fopen(MAILBOX_NAME, "r"); //Se abre modo lectura
                    if (mailbox == NULL) { //Error al abrir el archivo mailbox
                        perror("fopen");
                        exit(1);
                    }
                    char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en el mailbox
                    if (fgets(mensaje, MAXMSJ, mailbox) == NULL) {
                        perror("fgets");
                        exit(1);
                    }
                    printf("Mensaje del mailbox: %s\n", mensaje);

                    fclose(mailbox); //Se produce el evento IN_CLOSE_NOWRITE que se detectará en el proceso padre
                }
                else if (event->mask & IN_DELETE_SELF) { //Se ha detectado en la cola de eventos el evento IN_DELETE_SELF
                    exit(0); //Se termina la ejecución del hijo 
                }
                ptr += sizeof(struct inotify_event) + event->len;
            }  
        }
        close(fd);
    }
    else { //CÓDIGO DEL PROCESO PADRE:

        //Se inicializa inotify para el padre para detectar cuando el hijo termine de leer el mailbox
        int fd = inotify_init();
        if (fd == -1){
            perror("inotify_init");
            exit(1);
        }
        //Se añade el "watch" (monitor) del mailbox
        int wd = inotify_add_watch(fd, MAILBOX_NAME, IN_CLOSE_NOWRITE); //IN_CLOSE_NOWRITE: se cierra fichero/directorio abierto (no para escritura)

        char buffer[EVENT_BUF_LEN]; //Donde se irá almacenando la cola de eventos detectados en el proceso padre

        while(1) { //Bucle del proceso padre: 

            printf("Introduzca el mensaje a almacenar en el mailbox:\n");
            printf("> ");

            //SE LEE EL INPUT 
            char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en la entrada éstandar
            if(fgets(mensaje, MAXMSJ, stdin) == NULL) { //Si el mensaje leido contiene solo el carácter fin de fichero (EOF/CTRL+D)
                remove(MAILBOX_NAME); //Se borra el mailbox, se produce el evento IN_DELETE_SELF que se detectará en el proceso hijo 
                exit(0); //Se termina la ejecución del padre
            }

            //SE ESCRIBE EN EL MAILBOX:
            mailbox = fopen(MAILBOX_NAME, "w"); //Se abre modo escritura
            if (mailbox == NULL) { //Error al abrir el archivo mailbox
                perror("fopen");
                exit(1);
            }
            if (fputs(mensaje, mailbox) < 0 ) { //Error al escribir en el mailbox
                perror("fputs");
                exit(1);
            }

            fclose(mailbox); //Se produce el evento IN_CLOSE_WRITE, que se detectará en el proceso hijo

            //SE ESPERA A QUE EL HIJO CIERRE EL MAILBOX DESPUES DE LEER Y PRINTEAR EL MENSAJE (evento IN_CLOSE_NOWRITE):
            ssize_t length = read(fd, buffer, EVENT_BUF_LEN); 
            if (length == -1 && errno != EINTR) {
                perror("Error al leer eventos");
                break;
            }
            for (char *ptr = buffer; ptr < buffer + length; ) {
                struct inotify_event *event = (struct inotify_event *) ptr;

                if (event->mask & IN_CLOSE_NOWRITE) { //Se ha detectado en la cola de eventos el evento IN_CLOSE_NOWRITE
                    printf("Evento IN_CLOSE_NOWRITE detectado en el proceso padre\n");

                    sleep(pausaSecs); //Una vez se detecta el evento se repetirá la comunicación tras una pausa <PAUSASECS>
                }
                ptr += sizeof(struct inotify_event) + event->len;
            }
        }
        close(fd);
    }
}
