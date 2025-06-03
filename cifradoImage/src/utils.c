#include "../headers/utils.h"
#include <stdio.h>

// Construye la ruta concatenando directorio + nombre de archivo, evitando overflow
const char *build_path_static( const char *filename) {
    static char path[512];  // buffer estático, se mantiene entre llamadas
    snprintf(path, sizeof(path), "%s/%s", "./out", filename);
    return path;  // devuelve puntero al buffer estático
}
