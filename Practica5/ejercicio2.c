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
#include <fcntl.h>
#include <sys/file.h>

#define MAXMSJ 100 //Tamaño max del mensaje
#define N 3 //Se crearán N (en este caso 3) procesos hijo
#define MAILBOX_NAME "mailbox.txt"
#define EVENT_BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

void hijo(int i) {

    //Se inicializa inotify para el hijo (i) para detectar cuando el padre termine de escribir el mailbox
    int fd = inotify_init();
    if (fd == -1){
        perror("inotify_init");
        exit(1);
    }
    //Se añade el "watch" (monitor) del mailbox
    int wd = inotify_add_watch(fd, MAILBOX_NAME, IN_CLOSE_WRITE | IN_DELETE_SELF); //IN_CLOSE_WRITE: se cierra fichero abierto (para escritura) / IN_DELETE_SELF : se elimina fichero/directorio que se está monitorizando (para cuando EOF, mailbox se borra y esto notifica al proceso hijo)

    //printf("hijo %i, tamaño buffer %ld\n",i,EVENT_BUF_LEN);
    char buffer[EVENT_BUF_LEN]; //Donde se irá almacenando la cola de eventos detectados en el proceso hijo (i)

    while(1) { //Bucle del proceso hijo (i)
        
        //SE ESPERA A QUE EL PADRE CIERRE EL MAILBOX TRAS ESCRIBIR EN ESTE (evento IN_CLOSE_WRITE)
        ssize_t length = read(fd, buffer, EVENT_BUF_LEN); 
        if (length == -1 && errno != EINTR) {
            perror("Error al leer eventos");
            break;
        }
        for (char *ptr = buffer; ptr < buffer + length; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;

            if (event->mask & IN_CLOSE_WRITE) { //Se ha detectado en la cola de eventos el evento IN_CLOSE_WRITE
                //printf("Evento IN_CLOSE_WRITE detectado en el proceso hijo %d con PID: %d\n", i, getpid());

                int fd_mailbox = open(MAILBOX_NAME, O_RDONLY); //Se abre el mailbox como descriptor de fichero para llamar a fcntl
                if (fd_mailbox == -1) {
                    perror("open");
                    exit(1);
                }
                // adquirimos lock
                if (flock(fd_mailbox, LOCK_EX | LOCK_NB) == 0) { //Si el proceso hijo (i) obtiene el lock, porque ningún otro hijo ha bloqueado el fichero previamente

                    char msg[MAXMSJ + 1];
                    int n = read(fd_mailbox, msg, MAXMSJ); //Se produce el evento IN_ACCESS en el mailbox (solo si hijo (i) ha obtenido el lock)
                    if (n > 0) {
                        msg[n] = '\0';
                        printf("[Hijo %i] Mensaje recibido: %s\n", i, msg);
                    }
                    if (flock(fd_mailbox, LOCK_UN) == -1) {
                        perror("flock");
                        close(fd_mailbox);
                        exit(-1);
                    }
                }
                close(fd_mailbox); 

            } else if (event->mask & IN_DELETE_SELF) { //Se ha detectado en la cola de eventos el evento IN_DELETE_SELF
                close(fd);
                exit(0); //Se termina la ejecución del hijo (i)
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    close(fd);
    exit(0); //Finaliza el proceso hijo (i)
}

void padre(int pausaSecs){

    FILE* mailbox;

    //Se inicializa inotify para el padre para detectar cuando algún hijo termine de leer el mailbox
    int fd = inotify_init();
    if (fd == -1){
        perror("inotify_init");
        exit(1);
    }
    //Se añade el "watch" (monitor) del mailbox
    int wd = inotify_add_watch(fd, MAILBOX_NAME, IN_ACCESS); //IN_CLOSE_NOWRITE: se cierra fichero/directorio abierto (no para escritura)

    char buffer[EVENT_BUF_LEN]; //Donde se irá almacenando la cola de eventos detectados en el proceso padre

    while(1) { //Bucle del proceso padre: 

        printf("Introduzca el mensaje a almacenar en el mailbox:\n");
        printf("> ");

        //SE LEE EL INPUT 
        char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en la entrada éstandar
        if(fgets(mensaje, MAXMSJ, stdin) == NULL) { //Si el mensaje leido contiene solo el carácter fin de fichero (EOF/CTRL+D)
            remove(MAILBOX_NAME); //Se borra el mailbox, se produce el evento IN_DELETE_SELF que se detectará en los procesos hijo
            //Se espera que terminen los hijos
            for (int i = 0; i < N; ++i) {
                wait(NULL);
            } 
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

        fclose(mailbox); //Se produce el evento IN_CLOSE_WRITE, que se detectará en los procesos hijo

        //SE ESPERA A QUE ALGUN HIJO ACCEDA AL MAILBOX (evento IN_ACCESS) (todos los hijos cerrarán el mailbox independiemente de que accedan o no a él, por lo que ya no se podría utilizar el evento IN_CLOSE_WRITE)
        ssize_t length = read(fd, buffer, EVENT_BUF_LEN); 
        if (length == -1 && errno != EINTR) {
            perror("Error al leer eventos");
            break;
        }
        for (char *ptr = buffer; ptr < buffer + length; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;

            if (event->mask & IN_ACCESS) { //Se ha detectado en la cola de eventos el evento IN_CLOSE_NOWRITE
                printf("Evento IN_ACCESS detectado en el proceso padre\n");

                sleep(pausaSecs); //Una vez se detecta el evento se repetirá la comunicación tras una pausa <PAUSASECS>
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    close(fd);
}

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

    //Se crean N procesos hijo:
    pid_t pids_hijos[N];
    for (int i = 0; i < N; i++) {
        pids_hijos[i] = fork();
        if(pids_hijos[i] == -1){
            perror("fork");
            exit(1);
        } else if(pids_hijos[i] == 0) { //CODIGO DEL PROCESO HIJO (i)
            hijo(i);
        }
    }

    padre(pausaSecs); //CODIGO DEL PROCESO PADRE (igual que ej1, pero el padre detectará en evento IN_ACCESS)
}