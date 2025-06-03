#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

// Función para abrir un archivo
FILE *abrirArchivo(const char *nombreArchivo, const char *modo);

// Función para leer el contenido de un archivo (devuelve un puntero a una cadena con el contenido)
char *leerArchivo(FILE *archivo);

// Función para escribir una cadena en un archivo
int escribirArchivo(FILE *archivo, const char *contenido);

// Función para cerrar un archivo
void cerrarArchivo(FILE *archivo);

char *convertirMinusculas(const char *cadena);

#endif // UTILS_H
