#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

int periodo;
int segs_totales = 0;
char* mensaje = "Mensaje default";

void manejador(int sig) {
    if(sig == SIGALRM){
        segs_totales += periodo; //se suman los segundos totales
        printf("%s\n", mensaje); //se printea el mensaje
        alarm(periodo); 
    }
    else if(sig == SIGTERM){
        printf("Segundos totales transcurridos: %d\n", segs_totales); //se printean los segundos transcurridos
        exit(0); //se termina la ejecucion del programa
    }
    else if(sig == SIGINT) {
        //No hace nada, CTRL+C se tiene que ignorar
    }
}

int main (int argc, char *argv[]) {

    if (argc == 3) {
        mensaje = argv[2];
    }
    if (argc < 2 || argc > 3) {
        perror("Invalid argument\n");
        exit(1);
    }

    periodo = atoi(argv[1]);

    //Estructura sigaction
    struct sigaction signalAct; 
    sigemptyset(&signalAct.sa_mask);// Inicializa conjunto vacio de señales bloqueadas
    signalAct.sa_flags = 0; //No se utilizara ningun modificador

    sigset_t conjunto; //Conjunto de señales
    sigemptyset(&conjunto); //Se inicializa el conjunto de señales como vacio
    sigaddset(&conjunto, SIGALRM); //Se añade la señal SIGALARM al conjunto de señales
    sigaddset(&conjunto, SIGTERM); //Se añade la señal SIGTERM al conjunto de señales
    sigaddset(&conjunto, SIGINT); //Se añade la señal SIGINT al conjunto de señales

    //Configuracion del manejador:
    signalAct.sa_handler = manejador; 
    sigaction(SIGALRM, &signalAct, NULL);
    sigaction(SIGTERM, &signalAct, NULL);
    sigaction(SIGINT, &signalAct, NULL);

    //Para cuando se quiera terminar la ejecución del programa a través del comando kill (que capturará la señal SIGTERM) el usuario sepa el ID del proceso
    printf("Programa iniciado con PID: %d\n", getpid());

    alarm(periodo); 

    while (1) { //Bucle infinito en espera de las señales
        pause(); //duerme el proceso en curso hasta que reciba una señal.
    }

    return 0;
}