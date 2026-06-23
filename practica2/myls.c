#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>  // Para tipos como mode_t y macros de S_ISREG/S_ISLNK
#include <sys/stat.h>   // Para lstat y macros relacionadas con tipos de ficheros
#include <dirent.h> // Para dirent

#define BUFFER_SIZE 512

enum opciones { NOOP, OPCION_F, OPCION_L, OPCION_D, OPCION_LD, OPCION_XD};

int contarLineas(const char* filename){
    FILE* file = fopen(filename, "r");

    if(file == NULL) return -1;

    int lineas = 0;
    char c;
    while((c = fgetc(file)) != EOF){
        if(c == '\n') lineas++;
    }

    fclose(file);
    return lineas + 1;

}

int buscarTxt(const char* filename){
    FILE *file = fopen(filename, "r");
    if(file == NULL) return 0;  // No se pudo abrir --> binario

    unsigned char buffer[BUFFER_SIZE];
    size_t leidos = fread(buffer, 1, BUFFER_SIZE, file);
    fclose(file);

    if (leidos == 0) return 1; // Un archivo vacío lo tratamos como texto

    // Revisamos los caracteres leídos
    for (size_t i = 0; i < leidos; i++) {
        unsigned char c = buffer[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            return 0; // Contiene caracteres no imprimibles, es binario
        }
    }
    return 1; // Parece texto

}

void optionFL(const char* filename, int flag){
    // Obtener información del fichero origen
        struct stat info;
        if (lstat(filename, &info) == -1){
            perror("Error al obtener información del fichero origen");
            exit(EXIT_FAILURE);
        }
        if (S_ISREG(info.st_mode)) {
            int esTxt = 0;
            if(flag == OPCION_L){   // si está la opción -l
                esTxt = buscarTxt(filename);
            }
            if(info.st_size > 1024){
                if(flag == OPCION_F || !esTxt){
                    printf("%s (inodo %i, %i Kib)\n", filename, info.st_ino, info.st_size / 1024);

                }
                else{
                    int lineas = contarLineas(filename);
                    printf("%s (inodo %i, %i Kib, %i lineas)\n", filename, info.st_ino, info.st_size / 1024, lineas);

                }
            }
            else{
                if(flag == OPCION_F || !esTxt){
                    printf("%s (inodo %i, %i bytes)\n", filename, info.st_ino, info.st_size);

                }
                else{
                    int lineas = contarLineas(filename);
                    printf("%s (inodo %i, %i bytes, %i lineas)\n", filename, info.st_ino, info.st_size, lineas);

                }
            }
        }
}

#include <dirent.h>

const char* tipoArchivo(unsigned char tipo) {
    switch(tipo){
        case DT_BLK: return "Bloque de datos";
        case DT_CHR: return "Dispositivo de carácteres";
        case DT_DIR: return "Directorio";
        case DT_FIFO: return "Tubería";
        case DT_LNK: return "Enlace simbólico";
        case DT_REG: return "Archivo regular";
        case DT_SOCK: return "Socket";
        case DT_UNKNOWN: return "No determinado";
        default: return "Fallo";
    }
}


void optionD(const char* filename, int flag){

    struct stat info;
    if (lstat(filename, &info) == -1){
        perror("Error al obtener información del fichero origen");
        exit(EXIT_FAILURE);
    }

    if(S_ISDIR(info.st_mode)){
        if(access(filename, R_OK | X_OK) == 0) {    // Ver que es un directorio con permiso de escritura y ejecución
            struct dirent* file;
            DIR* dir = opendir(filename);
            while((file = readdir(dir)) != NULL){
                if(flag == OPCION_D){   // Lista todo lo del directorio
                    const char *tipo = tipoArchivo(file->d_type);
                    printf("Nombre: %s \tTipo: %s\n", file->d_name, tipo);
                }
                else if(flag == OPCION_LD  && file->d_type == DT_REG){   // Lista solo ficheros regulares
                    char ruta_completa[1024];   // hacer toda la ruta del directorio/archivo
                    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", filename, file->d_name);
                    
                    if(access(ruta_completa, R_OK) == 0){ // Ver si tiene permiso de lectura
                        const char *tipo = tipoArchivo(file->d_type);
                        int lineas = contarLineas(ruta_completa);
                        printf("Nombre: %s \tTipo: %s \tNº de lineas: %i\n", file->d_name, tipo, lineas);
                    }
                } 
                else if(flag == OPCION_XD && file->d_type == DT_REG){
                    char ruta_completa[1024];
                    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", filename, file->d_name);
                    if(access(ruta_completa, X_OK) == 0){
                        const char* tipo = tipoArchivo(file->d_type);
                        printf("Nombre: %s\tTipo: %s\n", file->d_name, tipo);
                    }
                }
            }
            closedir(dir);
        }

    }
}
int main(int argc, char *argv[]) {
    int opcion;
    char *filename = NULL;
    int flag = NOOP;
    int list = 0;

    // Definir las opciones largas
    static struct option long_options[] = {
        {"file", required_argument, NULL, 'f'},
        {"lines",no_argument, NULL, 'l'},
        {"directory", no_argument, NULL, 'd'},
        {"exe", no_argument, NULL, 'x'},
        {0, 0, 0, 0} // Fin de la lista
    };

    while ((opcion = getopt_long(argc, argv, "f:ldx", long_options, NULL)) != -1) {
        switch (opcion) {
            case 'f':   // -f filename ---> ver el tipo de archivo, etc.
                if(flag == NOOP){
                    flag = OPCION_F;
                }
                filename = optarg;
                break;
            case 'l':   // si el archivo es de tipo txt contar las lineas
                if(flag == OPCION_D){
                    flag = OPCION_LD;
                }
                else{
                    flag = OPCION_L;
                }
                break;
            case 'd':   // ver que es un directorio con permisos de escritura y ejecución y mostrar las cosas
                if(flag == OPCION_L){
                    flag = OPCION_LD;
                }
                else{
                    flag = OPCION_D;
                }
                break;
            case 'x':   // ver los archivos regulares y ejecutables de un directorio
                    flag = OPCION_XD;
                break;
            default:
                fprintf(stderr, "Error: opción no esperada\n");
                exit(1);
        }
    }

    if( flag == OPCION_F || flag == OPCION_L){        
        optionFL(filename, flag);
    }
    else if(flag == OPCION_D || flag == OPCION_LD || flag == OPCION_XD){
        optionD(filename, flag);
    }
    return 0;
}