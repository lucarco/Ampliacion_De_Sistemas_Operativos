#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

enum opciones { NOOP, OPCION_H, OPCION_O};

void usage(char **argv){
    printf("Uso: %s [-h] [-o filename]\n", argv[0]);
    printf("   -h          : imprime esta ayuda\n");
    printf("   -o filename : guarda la fecha en el fichero filename\n");    
}

int main(int argc, char *argv[]) {
    int opcion;
    char *filename = NULL;
    int flag = NOOP;

    while ((opcion = getopt(argc, argv, "ho:")) != -1) {
        switch (opcion) {
            case 'h':
                flag = OPCION_H;
                usage(argv);
                exit(0);
            case 'o':
                filename = optarg;
                flag = OPCION_O;
                break;
            case ':':
                fprintf(stderr, "Error: no se ha filename en la opción -o\n");
                exit(1);
            case '?':  
                fprintf(stderr, "Error: opción -%c no válida\n", (unsigned char) optopt);
                exit(1);
            default:
                fprintf(stderr, "Error: opción no esperada\n");
                exit(1);
        }
    }

    // Obtener la fecha y hora actual
    time_t t;
    struct tm *tm_info;
    char buffer[30];

    time(&t);
    tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

    // Manejo de salida según la opción especificada
    if (flag == OPCION_O) {
        FILE *file = fopen(filename, "w");
        if (file == NULL) {
            fprintf(stderr, "Error: no se pudo abrir %s\n", filename);
            exit(1);
        }
        fprintf(file, "Fecha y hora actual: %s\n", buffer);
        fclose(file);
        printf("Fecha y hora actual guardada en %s\n", filename);
    } else {
        printf("Fecha y hora actual: %s\n", buffer);
    }

    // Comprobar si hay argumentos adicionales no reconocidos
    if (optind < argc) {
        printf("Advertencia: Los siguientes argumentos no son opciones válidas:\n");
        for (int i = optind; i < argc; i++) {
            printf(" - %s\n", argv[i]);
        }
    } else {
        printf("No hay argumentos adicionales.\n");
    }

    return 0;
}