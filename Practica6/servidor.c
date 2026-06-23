
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
#include <syslog.h>

#include "sfile.h"

static char PID_FILE_PATH[] = "/home/usuarioso/pid_deamon"; //Cambiar a /var/run/pid_deamon
const char LOGNAME[] = "DAEMON_MSG";

// Variable global:
struct sigevent sev;

void turn_daemon() {  

    //Si salta algún error en el proceso de creación del demonio no se verá por pantalla => Se podría usar syslog en vez de perror() para que los mensajes se almacenen en directorios especiales como /var/log

    //1º Cerrar todos los descriptores de fichero abiertos excepto stdin, stdout y stderr
    DIR* directorio = opendir("/proc/self/fd");
    struct dirent *entrada;
    while((entrada = readdir(directorio)) != NULL) { 
        int fd = atoi(entrada->d_name);
        if(fd > 2) close(fd);  //No cierra stdin, stdout y stderr, fd = 0 fd = 1, fd = 2
    }
    closedir(directorio);
    
    //2º Restablecer todos los manejadores de señales al manejador por defecto
    struct sigaction sa; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sa.sa_handler = SIG_DFL;
    for(int signal = 1; signal < NSIG; signal++){ //_NSIG representa el número de señales definidas en el sistema y puede utilizarse para recorrer todas las señales posibles.
        sigaction(signal, &sa, NULL);
    }

    //3º Se desbloquea cualquier señal que pudiera estar bloqueada
    sigset_t emptySet;
    sigemptyset(&emptySet); //Se inicializa el conjunto de señales como vacio
    sigprocmask(SIG_SETMASK, &emptySet, NULL);  //SIG_SETMASK => The set of blocked signals is set to the argument set.

    //4º Eliminar variables de entorno innecesarias para la operación del demonio.
    //clearenv(); //Elimina todas las varibles del entorno
    //putenv(PATH_MAX); 

    //Creación de una tubería que se encargará de la comunicación entre el proceso demonio y el proceso original, para notificar al original cuando finalice la inicialización del demonio
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1){
        perror("pipe");
        exit(1);
    }

    //5º Double fork trick
    pid_t child_id = fork();
    if (child_id == -1) {
        perror("fork(1)");
        exit(1);
    }
    else if (child_id == 0) { //CODIGO DEL PROCESO HIJO (padre del proceso demonio)

        //Se crea una nueva sesion, para que este proceso (hijo) sea el lider de grupo y de esta sesión y pueda así desconectarse del terminal
        if (setsid() == -1) {
            perror("setsid");
            exit(1);
        }
        
        //Se vuelve a crear un nuevo proceso con un segundo fork() que será el que se convierta finalmente en demonio
        //(el demonio, al no ser líder de sesión, si abre un termina no se convierte en terminal de control.)
        pid_t daemon_id = fork();
        if (daemon_id == -1) {
            perror("fork(2)");
            exit(1);
        }
        else if (daemon_id == 0) { // CÓDIGO DEL PROCESO DEMONIO
            
            //1º Conectar stdin, stdout y stderr al dispositivo /dev/null. => usando dup2() para redirigir 
            //para evitar que el demonio interactúe con la terminal del usuario o que genere salidas no controladas.
            int fd = open("/dev/null", O_RDWR);
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);

            //2º Restablecer la máscara umask a 0 (para que los permisos establecidos con open(), mkdir() u otras llamadas similares controlen directamente dichos permisos.)
            umask(0);

            //3º Cambiar el directorio de trabajo actual al directorio raíz (/).
            chdir("/");

            //4º Escribir el PID del demonio en el fichero PID asociado a este demonio, se evita así generar múltiples instancias simultáneas del mismo demonio
            //(Un archivo PID (típicamente en /var/run/ o /run/) contiene el PID (Process ID) del demonio en ejecución.
            int pid_fd = open(PID_FILE_PATH, O_RDWR | O_CREAT, 0644);
            if(pid_fd == -1){
                perror("open (pid file)");
                exit(1);
            }
            dprintf(pid_fd, "%d\n", getpid());//Escribir el PID del demonio en el fichero PID asociado
            // Para evitar condiciones de carrera se debe proteger el acceso a este fichero con locks de ficheros (con flock()
            if (flock(pid_fd, LOCK_EX) == -1) {
                perror("Otra instancia del daemon ya se está ejecutando");
                close(pid_fd);
                exit(1);
            }
            close(pid_fd);
           
            //5º Renunciar a privilegios si es posible y aplicable
            if (geteuid() == 0) { // El proceso se está ejecutando como root se puede renunciar a privilegios. (geteuid() returns the effective user ID of the calling process, 0 => root)
                char username[] = "nobody"; //usuario especial del sistema Unix/Linux que representa a "nadie", es decir, un usuario sin privilegios
                struct passwd* pw = getpwnam(username); //estructura passwd que contiene información sobre el usuario especificado.
                if (pw == NULL) {
                    perror("getpwnam");
                    exit(1);
                }
                if (setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
                    perror("setgid o setuid");
                    exit(1);
                }
            }

            //6º Notificar al proceso original que la inicialización está completa. A través de la tubería
            close(pipe_fds[0]); //Se cierra el extremo de lectura
            char msg[] = "listo";
            write(pipe_fds[1], msg, sizeof(msg)); //Se escribe "listo" en el extremo de escritura
            close(pipe_fds[1]); 
        }
        else {//Resto del código del proceso padre del proceso demonio (proceso hijo originalmente)
            //el proceso hijo finaliza con exit(). De este modo, el padre del demonio pasa a ser el proceso init (actualmente el proceso systemd correspondiente).
            exit(0);
        }
    }
    else { //CODIGO DEL PROCESO PADRE (original)

        //Se espera a que el proceso demonio notifique que su inicialización está completada
        close(pipe_fds[1]); //Cierra el extremo de escritura del pipe
        char msg[6];
        read(pipe_fds[0], msg, sizeof(msg)); //Se lee "listo" del extremo de lectrura, read => operación bloqueante, hasta que no realiza la lectura bloquea al proceso original
        close(pipe_fds[0]);

        //El proceso original, una vez confirmando que la inicialización del demonio ha sido correcta puede finalizar.
        return;
    }
}

void handle_error(char *err, mqd_t qd_req) { //Muestra el error en syslog y se encarga de borrar la cola de mensajes 
    syslog(LOG_USER | LOG_ERR, "%s", err);
    mq_close(qd_req);
    mq_unlink(req_queue);
    unlink(PID_FILE_PATH); //Borra tmb el archivo del pid del demonio
    closelog();
    exit(1);
}

static void tfunc(union sigval sv) { //Función que ejecuta el hilo una vez se lanza tras haber recibido una notificación

    //SE RECIBE EL MENSAJE CON LA SOLICITUD DE ARCHIVO (mensaje request)
    mqd_t qd = *((mqd_t *) sv.sival_ptr); //Descriptor de la cola que pasa como parámetro
    struct mq_attr attr_rq; 
    if (mq_getattr(qd, &attr_rq) == -1) { //Estructura mq_attr d la cola de mensajes para poder acceder a attr.mq_msgsize ==  REQ_MSG_SIZE (se podría usar directamente)
        handle_error("mq_getattr", qd); 
    }
    struct requestMsg request; //Sino en attr_rq.mq_msgsize probar con sizeof(request)
    memset(&request, 0, sizeof(request));
    ssize_t length = mq_receive(qd, (char *)&request, attr_rq.mq_msgsize, 0); // Se recibe el mensaje más antiguo con la mayor prioridad de la mqueue. => modo no bloqueante,
    if (length == -1) {
        handle_error("mq_receive", qd);
        //Re-registrar notificación?¿
    }

    //SE PROCESA LA SOLICITUD:
    char* fileName = request.pathname;
    int clientPid = request.clientId; 
    syslog(LOG_USER | LOG_INFO, "Solicitud de archivo %s, realizada por el cliente %d, recibida", fileName, clientPid);
    //Se abre la cola de mensajes de respuesta modo escritura
    mqd_t qd_res = mq_open(res_queue, O_WRONLY);  
    if (qd_res == (mqd_t)-1) {
        handle_error("mq_open", qd);
    }
    struct responseMsg response;
    memset(&response, 0, sizeof(response));
    struct mq_attr attr_rs; 
    if (mq_getattr(qd, &attr_rs) == -1) { //Estructura mq_attr d la cola de mensajes para poder acceder a attr_rs.mq_msgsize ==  RESP_MSG_SIZE (se podría usar directamente)
        handle_error("mq_getattr", qd); 
    }
    //Se abre el archivo:
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) { //Error al leer el archivo -> se informa a ciente
        response.mtype = RESP_MT_FAILURE; //Tipo de mensaje de respuesta => error
        strcpy(response.data, "Error al abrir el archivo");
        mq_send(qd_res, (char *)&response, sizeof(response), 0);
        close(qd_res);
        handle_error("open", qd);
    }
    //Si puede abrirlo, lee su contenido y lo envía en varios mensajes al cliente
    ssize_t bytes_read;
    response.mtype = RESP_MT_DATA;
    while ((bytes_read = read(fd, response.data, attr_rs.mq_msgsize)) > 0) {
        if (mq_send(qd_res, (char *)&response, attr_rs.mq_msgsize, 0) == -1) {
            break;
        }
    }
    // Enviar mensaje de fin
    response.mtype = RESP_MT_END;
    mq_send(qd_res, (char *)&response, sizeof(response.mtype), 0);
    
    // Limpieza
    close(fd);
    close(qd_res);

    //Se re-registra la notificación para cuando llegue otro mensaje request se lance otro hilo
    if (mq_notify(qd, &sev) == -1) { 
        close(qd_res);
        handle_error("mq_notify", qd);
    }
}

int main(int argc, char *argv[]) {

    //PROCESO SERVIDOR SE TRANSFORMA EN DEMONIO:
    turn_daemon();
    //Una vez el proceso se ha transformado en demonio se tendrá que usar syslog para comunicarse con el usuario
    openlog(LOGNAME, LOG_PID, LOG_USER); //(cat /var/log/syslog para ver los logs)
    syslog(LOG_INFO, "Demonio inicializado correctamente");

    //SE SE CREA COLA (request) DE MENSAJES POSIX:
    //Se configuran los atributos de la cola (struct mq_attr)
    struct mq_attr attr;
    attr.mq_curmsgs = 0; //# mensajes actualmente
    attr.mq_flags = O_NONBLOCK; // Flags: 0 ó O_NONBLOCK, modo no bloqueante (es útil para vaciar la cola de mensajes sin bloquear una vez que está vacía).
    attr.mq_maxmsg = MAX_MESSAGES; // Max. # mensajes
    attr.mq_msgsize = REQ_MSG_SIZE; // Max. tamaño (bytes)
    // Crear/abrir la cola de mensajes request
    //Se crea la cola de mensajes request (donde el servidor recibira las solicitudes de archivo por parte del cliente)
    mq_unlink(req_queue); //Por si ya existe
    mqd_t qd_req = mq_open(req_queue, O_RDONLY | O_CREAT, 0644, &attr); //Se crea la cola de mensajes request modo lectura
    if(qd_req == (mqd_t)-1 ){ 
        syslog(LOG_ERR, "mq_open");
        unlink(PID_FILE_PATH);
        exit(1);
    }

    //Uso de mq_notify: => Se registra una notificación que lanzará un hilo (que ejecutará la función tfunc) cuando la cola de mensajes pase de estar vacía a no vacía por 1era vez
    //Se configuran los datos del struct sigevent
    memset(&sev, 0, sizeof(struct sigevent)); // Inicializar la estructura a cero
    sev.sigev_notify = SIGEV_THREAD; //Se registra el método de notificación SIGEV_THREAD
    sev.sigev_notify_function = tfunc; //Función que ejecutará el hilo
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_ptr = &qd_req; //Campo que permite pasar un dato o puntero a la función que se ejecutará cuando se dispare la notificación 
    // Registrar la notificación
    if (mq_notify(qd_req, &sev) == -1) {
        handle_error("mq_notify", qd_req); 
    }

    // Bucle principal del demonio
    while(1) {
        pause(); // Esperar hasta que llegue alguna notificación
    }

    mq_close(qd_req);
    mq_unlink(req_queue);
    closelog();

    exit(0);
}