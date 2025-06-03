#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>


/**
 * @brief Construye una ruta de archivo completa a partir de un nombre de archivo.
 * 
 * @param filename Nombre del archivo.
 * @return const char* Ruta completa generada (puntero estático, no liberar).
 */
const char *build_path_static(const char *filename);

/**
 * @brief Muestra por pantalla el mensaje de uso del programa.
 */
void printUsage();

/**
 * @brief Inicia el modo interactivo del programa.
 */
void interactiveMode();

#endif
