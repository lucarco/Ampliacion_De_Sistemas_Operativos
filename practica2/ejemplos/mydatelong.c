#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

enum opciones { NOOP, OPCION_H, OPCION_O};

int verbose_flag = 0; // Por defecto, modo no verbose

void usage(char **argv){
    printf("Uso: %s [-h | --help] [-o filename | --output filename] [--verbose]"
            "\n", argv[0]);
    printf("   -h, --help            : imprime esta ayuda\n");
    printf("   -o, --output filename : guarda la fecha en el fichero filename\n");
    printf("   --verbose             : muestra mensajes de salida\n");
}
int main(int argc, char *argv[]) {
    int opcion;
    char *filename = NULL;
    int flag = NOOP;
    FILE *file = stdout; // Salida predeterminada stdout

    // Definir las opciones largas
    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"output", required_argument, NULL, 'o'},
        {"verbose", no_argument, &verbose_flag, 1},  // Activa verbose
        {0, 0, 0, 0} // Fin de la lista
    };

    while ((opcion = getopt_long(argc, argv, "ho:", long_options, NULL)) != -1) {
        switch (opcion) {
            case 'h':
                flag = OPCION_H;
                usage(argv);
                exit(0);
            case 'o':
                filename = optarg;
                flag = OPCION_O;
                break;
            case 0:
                // opcion verbose: se actualiza el valor de verbose_flag 
                break;
            case ':':
                fprintf(stderr, "Error: falta filename en la opción -o\n");
                exit(1);
            case '?':  
                fprintf(stderr, "Error: opción no válida\n");
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

    // Si verbose está activado, mostramos fecha y hora completa. En caso
    // contrario solo fecha
    if (verbose_flag) {
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {

        strftime(buffer, sizeof(buffer), "%Y-%m-%d", tm_info); 
    }

    // salida
    if (flag == OPCION_O) {
        file = fopen(filename, "w");
        if (file == NULL) {
            fprintf(stderr, "Error: no se pudo abrir %s\n", filename);
            exit(1);
        }
        if (verbose_flag) {
            printf("Guardando fecha y hora actual en %s\n", filename);
        }
    }

    // mostrar fecha según el modo de verbosidad
    fprintf(file, "%s%s\n", (verbose_flag ? "Fecha y hora actual: " : ""), 
                             buffer);

    // Cerrar fichero si se abrió
    if (flag == OPCION_O) {
        fclose(file);
    }

    // Comprobar si hay argumentos adicionales no reconocidos (solo en modo verbose)
    if (verbose_flag && optind < argc) {
        printf("Advertencia: argumentos no válidos:\n");
        for (int i = optind; i < argc; i++) {
            printf(" %s\n", argv[i]);
        }
    }
    return 0;
}