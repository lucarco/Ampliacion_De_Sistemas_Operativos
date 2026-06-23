#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/file.h>
#include <pwd.h>
#include <mqueue.h>
#include <pthread.h>
//#include <asm-generic/sigi

#include "sfile.h"

int main(int argc, char *argv[]) {

    if (argc != 2 || strcmp(argv[1], "--help") == 0){ 
        fprintf(stderr,"%s pathname\n", argv[0]);
        exit(1);
    }

    struct requestMsg request; //Struct con el mensaje request (solicitud de archivo => que procesará el servidor)
    memset(&request, 0, sizeof(request));

    if (strlen(argv[1]) > sizeof(request.pathname) - 1){ //Si el nombre del archivo es muy largo, se informa al usuario
        fprintf(stderr,"pathname too long (max: %ld bytes)\n",
                (long) sizeof(request.pathname) - 1);
        exit(1);
    }

    //SE CREA LA COLA (response) DE MENSAJES POXIS (para recibir mensajes del servidor)
    //Se configuran los atributos de la cola (struct mq_attr)
    mq_unlink(res_queue); //Por si hay alguna cola con el mismo nombre ya creada
    struct mq_attr attr;
    attr.mq_curmsgs = 0; //# mensajes actualmente
    attr.mq_flags = 0; // Flags: 0 => bloqueante
    attr.mq_maxmsg = MAX_MESSAGES; // Max. # mensajes
    attr.mq_msgsize = RESP_MSG_SIZE; // Max. tamaño (bytes)
    //Se crea la cola de mensajes response (donde el cliente recibirá los mensajes originados al procesar la solicitud por parte del servidor)
    mqd_t qd_resp = mq_open(res_queue, O_RDONLY | O_CREAT, 0644, &attr); //Se crea/abre la cola de mensajes response modo escritura
    if(qd_resp == (mqd_t)-1 ){ 
        perror("mq_open 1");
        exit(1);
    }

    //SE MANDA LA SOLICITUD A TRAVES DE LA COLA (request) AL SERVIDOR:
    //Se abre la cola request modo escritura:
    mqd_t qd_req = mq_open(req_queue, O_WRONLY);  
    if (qd_req == (mqd_t)-1) {
        perror("mq_open 2");
        mq_close(qd_resp);
        mq_unlink(res_queue);
        exit(1);
    }
    //Se prepara la solucitud:
    request.clientId = getpid(); 
    request.mtype = 1; //No se hará nada en el server con esta info
    strncpy(request.pathname, argv[1], sizeof(request.pathname) - 1);
    request.pathname[sizeof(request.pathname) - 1] = '\0';
    //Se manda la solicitud:
    printf("Mandando solicitud al servidor para el archivo: %s\n", request.pathname);
    if (mq_send(qd_req, (char *)&request, REQ_MSG_SIZE, 0) == -1){
        perror("mq_send");
        mq_close(qd_resp);
        mq_unlink(res_queue);
        exit(1);
    }

    //SE ESPERA A LA RESPUESTA DEL SERVIDOR:
    struct responseMsg response; 
    memset(&request, 0, sizeof(request));

    //Recibe el primer mensaje:
    ssize_t length;
    length = mq_receive(qd_resp, (char *)&response, RESP_MSG_SIZE, 0);
    if (length == -1){
        perror("mq_receive");
       
        exit(1);
    }
    if (response.mtype == RESP_MT_FAILURE) { //Si el primer mensaje es de error => el archivo no se pudo abrir
        printf("%s\n", response.data);      
        mq_close(qd_resp);
        mq_unlink(res_queue);
        exit(1);
    }
    ssize_t totalBytes = length; //Los bytes del primer mensaje se añaden al número total de bytes
    int numMsgs; 
    for (numMsgs = 1; response.mtype == RESP_MT_DATA; numMsgs++) { //Se van recibiendo el resto de mensajes si los hay (hasta que salte uno tipo RESP_MT_END)
        memset(&response, 0, sizeof(response)); //Limpia el buffer de mensaje response
        length = mq_receive(qd_resp, (char *)&response, RESP_MSG_SIZE, 0);
        if (length == -1){
            perror("mq_receive");
            mq_close(qd_resp);
            mq_unlink(res_queue);
            exit(1);
        }
        totalBytes += length;
    }

    printf("Received %ld bytes (%d messages)\n", (long) totalBytes, numMsgs);
    mq_close(qd_req);
    mq_close(qd_resp);
    mq_unlink(res_queue);

    exit(0); 
}