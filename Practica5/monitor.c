#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define EVENT_BUF_LEN (sizeof(struct inotify_event) + NAME_MAX + 1)

void print_event(struct inotify_event *event) {
    printf("Evento en %i: %s; cookie %i ", event->wd, event->name[0] ? event->name : "", event->cookie);
    if (event->mask & IN_ACCESS) printf("IN_ACCESS ");
    if (event->mask & IN_MODIFY) printf("IN_MODIFY ");
    if (event->mask & IN_ATTRIB) printf("IN_ATTRIB ");
    if (event->mask & IN_CLOSE_WRITE) printf("IN_CLOSE_WRITE ");
    if (event->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
    if (event->mask & IN_OPEN) printf("IN_OPEN ");
    if (event->mask & IN_MOVED_FROM) printf("IN_MOVED_FROM ");
    if (event->mask & IN_MOVED_TO) printf("IN_MOVED_TO ");
    if (event->mask & IN_CREATE) printf("IN_CREATE ");
    if (event->mask & IN_DELETE) printf("IN_DELETE ");
    if (event->mask & IN_DELETE_SELF) printf("IN_DELETE_SELF ");
    if (event->mask & IN_MOVE_SELF) printf("IN_MOVE_SELF ");
    if (event->mask & IN_UNMOUNT) printf("IN_UNMOUNT ");
    if (event->mask & IN_Q_OVERFLOW) printf("IN_Q_OVERFLOW ");
    if (event->mask & IN_IGNORED) printf("IN_IGNORED ");
    if (event->mask & IN_ISDIR) printf("IN_ISDIR ");
    printf("\n");
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <fichero/directorio> [<fichero/directorio> ...]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    int inotify_fd = inotify_init();
    if (inotify_fd == -1) {
        perror("Error al inicializar inotify");
        return EXIT_FAILURE;
    }
    
    int *watch_descriptors = malloc((argc - 1) * sizeof(int));
    if (!watch_descriptors) {
        perror("Error al asignar memoria");
        close(inotify_fd);
        return EXIT_FAILURE;
    }
    
    for (int i = 1; i < argc; i++) {
        watch_descriptors[i-1] = inotify_add_watch(inotify_fd, argv[i], IN_ALL_EVENTS);
        if (watch_descriptors[i-1] == -1) {
            perror("Error al añadir monitorización");
        } else {
            printf("Monitoreando: %s con wd %i\n", argv[i],watch_descriptors[i-1]);
        }
    }
    
    char buffer[EVENT_BUF_LEN];
    while (1) {
        ssize_t length = read(inotify_fd, buffer, EVENT_BUF_LEN);
        if (length == -1 && errno != EINTR) {
            perror("Error al leer eventos");
            break;
        }
        
        for (char *ptr = buffer; ptr < buffer + length; ) {
            struct inotify_event *event = (struct inotify_event *) ptr;
            print_event(event);
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    
    free(watch_descriptors);
    close(inotify_fd);
    return EXIT_SUCCESS;
}
