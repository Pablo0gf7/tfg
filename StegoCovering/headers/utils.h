#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <sndfile.h>


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

void print_usage();
void calculate_mse_psnr(const char *original_file, const char *stego_file, const char *output_txt, sf_count_t samples_to_check);
char *read_text_file(const char *filename);
void save_message_to_file(const char *filename, const unsigned char *msg, size_t msg_len);


// Función para abrir un archivo
FILE *abrirArchivo(const char *nombreArchivo, const char *modo);

// Función para leer el contenido de un archivo (devuelve un puntero a una cadena con el contenido)
char *leerArchivo(FILE *archivo);

// Función para escribir una cadena en un archivo
int escribirArchivo(FILE *archivo, const char *contenido);

// Función para cerrar un archivo
void cerrarArchivo(FILE *archivo);

char *convertirMinusculas(const char *cadena);

#endif
