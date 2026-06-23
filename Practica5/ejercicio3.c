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

#define MAXMSJ 100 //Tamaño max del mensaje
#define N 3 //Se crearán N (en este caso 3) procesos hijo
#define EVENT_BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

//Variable global:
FILE* mailboxes[N]; //N ficheros mailbox: (mailbox0, mailbox1,... mailboxN)

void hijo(int i) {

    //Se inicializa inotify para el hijo (i) para detectar cuando el padre termine de escribir el mailbox<i>.txt (mailbox privado para la comunicación entre el padre y el proceso hijo (i))
    int fd = inotify_init();
    if (fd == -1){
        perror("inotify_init");
        exit(1);
    }
    //Se añade el "watch" (monitor) del mailbox (i)
    char mailboxName[12]; //mailbox<i>.txt
    sprintf(mailboxName, "mailbox%d.txt", i);
    inotify_add_watch(fd, mailboxName, IN_CLOSE_WRITE | IN_DELETE_SELF); //IN_CLOSE_WRITE: se cierra fichero abierto (para escritura) / IN_DELETE_SELF : se elimina fichero/directorio que se está monitorizando (para cuando EOF, mailbox(i) se borra y esto notifica al proceso hijo (i)
    
    char buffer[EVENT_BUF_LEN]; //Donde se irá almacenando la cola de eventos detectados en el proceso hijo (i)

    while(1) { //Bucle del proceso hijo (i)
        //SE ESPERA A QUE EL PADRE CIERRE EL MAILBOX (i) TRAS ESCRIBIR EN ESTE (evento IN_CLOSE_WRITE)
        ssize_t length = read(fd, buffer, EVENT_BUF_LEN); 
        if (length == -1 && errno != EINTR) {
            perror("read");
            break;
        }
        for (char *ptr = buffer; ptr < buffer + length; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;

            if (event->mask & IN_CLOSE_WRITE) { //Se ha detectado en la cola de eventos el evento IN_CLOSE_WRITE
                    
                //SE LEEN LOS DATOS DISPONIBLES EN EL MAILBOX (i):
                int fd_mailbox = open(mailboxName, O_RDONLY); //Se abre el mailbox como descriptor de fichero
                if (fd_mailbox == -1) {
                    perror("open");
                    exit(1);
                }
                char msg[MAXMSJ + 1];
                int n = read(fd_mailbox, msg, MAXMSJ); //Se produce el evento IN_ACCESS en el mailbox (solo si hijo (i) ha obtenido el lock)
                if (n > 0) {
                    msg[n] = '\0';
                    printf("[Hijo %d] ha leido: %s\n", i, msg);
                }
                close(fd_mailbox); 
            }
            else if (event->mask & IN_DELETE_SELF) { //Se ha detectado en la cola de eventos el evento IN_DELETE_SELF
                exit(0); //Se termina la ejecución del hijo (i)
            }
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    close(fd);
    exit(0); //Finaliza el proceso hijo (i)
}

void padre (int pausaSecs) {
 
    int fd = inotify_init(); //Un único descriptor inotify con múltiples watches (uno por mailbox)
    if (fd == -1){
        perror("inotify_init");
        exit(1);
    }
    //Se añaden N watchers (para detectar cuando alguno de los N hijos termine de leer su correspondiente mailbox)
    int wds[N];
    for (int i = 0; i < N; i++) {
        char mailboxName[12]; //mailbox<i>.txt
        sprintf(mailboxName, "mailbox%d.txt", i);
        //Se añade el "watch" (monitor) del mailbox<i>
        wds[i] = inotify_add_watch(fd, mailboxName, IN_ACCESS); 
    }
    char buffer[EVENT_BUF_LEN * N]; //Donde se irá almacenando la cola de eventos detectados en el proceso padre

    while(1) { //Bucle del proceso padre: 

        printf("Introduzca el mensaje a almacenar en los (N) mailboxes:\n");
        printf("> ");

        //SE LEE EL INPUT 
        char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en la entrada éstandar
        if(fgets(mensaje, MAXMSJ, stdin) == NULL) { //Si el mensaje leido contiene solo el carácter fin de fichero (EOF/CTRL+D)
            for(int i = 0; i < N; i++) { //Se borran todos los mailboxes => Se producirá el evento IN_DELETE_SELF en cada uno de estos, que hará que termine la ejecución del proceso hijo asociado
                char mailboxName[12]; 
                sprintf(mailboxName, "mailbox%d.txt", i);
                remove(mailboxName);
            }
            //Se espera que terminen los hijos
            for (int i = 0; i < N; ++i) {
                wait(NULL);
            } 
            exit(0); //Se termina la ejecución del padre
        }

        //SE ESCRIBE EN LOS (N) MAILBOXES:
        for (int i = 0; i < N; i++) {

            char mailboxName[12]; //mailbox<i>.txt
            sprintf(mailboxName, "mailbox%d.txt", i);
            //Se escribe el mensaje en el mailbox(i)
            mailboxes[i] = fopen(mailboxName, "w"); //Se abre modo escritura
            if (mailboxes[i] == NULL) { //Error al abrir el archivo mailbox
                perror("fopen");
                exit(1);
            }
            if (fputs(mensaje, mailboxes[i]) < 0 ) { //Error al escribir en el mailbox
                perror("fputs");
                exit(1);
            }
            fclose(mailboxes[i]); //Se produce el evento IN_CLOSE_WRITE del mailbox(i), que se detectará en el proceso hijo (i)  
        }

        //SE ESPERA A QUE EL TODOS LOS HIJOS CIERREN SU CORRESPONDIENTE MAILBOX DESPUES DE HABER LEÍDO Y PRINTEADO EL MENSAJE:
        int reads[N] = {0}; //Array que almacena si el mailbox(i) ha sido leído o no, se inicializan todos a "false"
        int numReads = 0;

        while (numReads < N) {

            ssize_t length = read(fd, buffer, EVENT_BUF_LEN); 
            if (length == -1 && errno != EINTR) {
                perror("Error al leer eventos");
                break;
            }
            for (char *ptr = buffer; ptr < buffer + length; ) {
                struct inotify_event *event = (struct inotify_event *) ptr;

                if (event->mask & IN_ACCESS) { //Se ha detectado en la cola de eventos el evento IN_ACCES
                    
                    //Comprobar para cual de los mailboxes se ha generado este evento, cada mailbox tiene su descriptor de watcher asociado:
                    for (int i = 0; i < N; i++) {
                        if (event->wd == wds[i] && !reads[i]) { //Si el watcher descriptor del mailbox(i) corresponde al watcher descriptor del evento generado y el mailbox(i) aun no se ha registrado como "leído"
                            printf("[PADRE] Hijo %d ha leido su mailbox\n", i);
                            reads[i] = 1;
                            numReads++;
                            break;
                        }
                    }
                }
                ptr += sizeof(struct inotify_event) + event->len;
            }
        }
        //Cuando se salga del bucle => Todos los mailboxes han sido leídos
        printf("Se han leido todos los mailboxes\n");
        printf("\n");
        sleep(pausaSecs); //Se repetirá la comunicación tras una pausa <PAUSASECS>
    }
    close(fd);
}

int main() { 

    //Se crean los N ficheros mailbox:  (mailbox0, mailbox1,... mailboxN) => Cada uno corresponderá a un mailbox privado para cada hijo
    for (int i = 0; i < N; i++) {
        char mailboxName[12]; //mailbox<i>.txt
        sprintf(mailboxName, "mailbox%d.txt", i);
        mailboxes[i] = fopen(mailboxName, "a");
        if (mailboxes[i] == NULL) {
            perror("fopen");
            exit(1);
        }
        fclose(mailboxes[i]);
    }

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
        }
        else if(pids_hijos[i] == 0) { 
            hijo(i); //CODIGO DEL PROCESO HIJO (i)
        }
    }
    //CODIGO DEL PADRE: 
   padre(pausaSecs);
}
