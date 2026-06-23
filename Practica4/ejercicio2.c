#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define MAXMSJ 15 //Tamaño max del mensaje
#define MAILBOX_NAME "mailbox.txt"

void manejador_padre(int signal) {
    printf("Padre: recibida la señal SIGUSR2 emitida por el hijo, listo para recibir mas mensajes\n");
}

void manejador_hijo(int signal) {
    if(signal == SIGUSR1){
        printf("Hijo: recibida la señal SIGUSR1 emitida por el padre, hay datos disponibles en 'mailbox'\n");
    }
    else if(signal == SIGTERM){
        printf("Hijo: recibida señal SIGTERM, terminando la ejecucion...\n");
        exit(0);
    }   
}

int main() { 

    pid_t pid_hijo, pid_padre;
    FILE* mailbox;
    int pausaSecs = 1; // Si PAUSASECS no esta definida por defecto se hara una pausa de 1 segundo

    char* varEntorno = getenv("PAUSASECS"); //Devuelve el valor asignado de la variable del entorno "PAUSASECS" (como cadena de caracteres)
    if(varEntorno != NULL) { 
        pausaSecs = atoi(varEntorno);
        if(pausaSecs == 0) {// Si da Error al convertir a entero 
            pausaSecs = 1; //Se asigna el valor por defecto
        }
    }

    //Estructura sigaction
    struct sigaction signalAct; 
    sigemptyset(&signalAct.sa_mask);// Inicializa conjunto vacio de señales bloqueadas
    signalAct.sa_flags = 0; //No se utilizara ningun modificador

    sigset_t set_padre, set_hijo; //Conjunto de señales "emitidas" por el padre y conjunto de señales "emitidas" por el hijo
    sigemptyset(&set_padre);
    sigemptyset(&set_hijo);
    sigaddset(&set_padre, SIGUSR1);
    sigaddset(&set_padre, SIGTERM);
    sigaddset(&set_hijo, SIGUSR2);

    //Configuracion de SIGUSR1 y SIGTERM: señales "emitidas" por el padre, recibidas por el hijo => manejador_hijo
    signalAct.sa_handler = manejador_hijo;
    sigaction(SIGUSR1, &signalAct, NULL);
    sigaction(SIGTERM, &signalAct, NULL);
    
    //Configuracion SIGUSR2: señal "emitida" por el padre que recibe el hijo => manejador_padre
    signalAct.sa_handler = manejador_padre;
    sigaction(SIGUSR2, &signalAct, NULL);

    pid_padre = getpid();
    //Se crea proceso hijo:
    pid_hijo = fork(); 

    if(pid_hijo == -1) { //Error al crear el proceso hijo
        perror("fork()");
        exit(1); 
    }
    else if(pid_hijo == 0) { //CÓDIGO DEL PROCESO HIJO

        while(1) { //Bucle del proceso hijo: Espera señal, lee archivo, envía señal SIGUSR2

            //SE ESPERA A LA SEÑAL SIGUSR1
            pause(); 

            //SE LEEN LOS DATOS DISPONIBLES EN EL MAILBOX:
            mailbox = fopen(MAILBOX_NAME, "r"); //Se abre modo lectura
            if (mailbox == NULL) { //Error al abrir el archivo mailbox
                perror("fopen");
                exit(1);
            }
            char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en el mailbox
            if (fgets(mensaje, MAXMSJ, mailbox) == NULL) {
                perror("fputs");
                exit(1);
            }
            printf("Mensaje del mailbox: %s\n", mensaje);

            //SE ENVÍA LA SEÑAL SIGUSR2 AL PROCESO PADRE
            fclose(mailbox);
            kill(pid_padre, SIGUSR2);
        }
    }
    else { //CÓDIGO DEL PROCESO PADRE:

        while(1) { //Bucle del proceso padre: Lee input, escribe en el archivo, envía señal SIGUSR1

            printf("Introduzca el mensaje a almacenar en el mailbox:\n");
            printf("> ");

            //SE LEE EL INPUT 
            char mensaje[MAXMSJ]; //Cadena donde se almacenará el mensaje leido en la entrada éstandar
            if(fgets(mensaje, MAXMSJ, stdin) == NULL) { //Si el mensaje leido contiene solo el carácter fin de fichero (EOF/CTRL+D)
                //El protocolo de comunicación se interrumpirá
                //El padre enviará al hijo la señal SIGTERM y terminará su ejecución
                kill(pid_hijo, SIGTERM);
                wait(NULL);//Se espera a que el hijo termine su ejecución
                exit(0); //Se termina tambien la ejecución del padre
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

            //SE ENVÍA LA SEÑAL SIGUSR1 AL PROCESO HIJO
            fclose(mailbox);
            kill(pid_hijo, SIGUSR1);
            
            pause(); //El proceso padre espera a la señal SIGUSR2 del hijo para volver a ejecutar el bucle y repetir el proceso de comunicación
            sleep(pausaSecs); //Una vez recibe la señal, repetirá la comunicación tras una pausa <PAUSASECS>
        }
    }
}