#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
// Función para abrir un archivo
FILE *abrirArchivo(const char *nombreArchivo, const char *modo)
{
    FILE *archivo = fopen(nombreArchivo, modo);
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo");
    }
    return archivo;
}

// Función para leer el contenido de un archivo
char *leerArchivo(FILE *archivo)
{
    if (archivo == NULL)
    {
        return NULL;
    }

    fseek(archivo, 0, SEEK_END);           // Mover el cursor al final del archivo
    long longitudArchivo = ftell(archivo); // Obtener la longitud del archivo
    rewind(archivo);                       // Volver al inicio del archivo

    char *contenido = (char *)malloc((longitudArchivo + 1) * sizeof(char)); // +1 para el terminador '\0'
    if (contenido == NULL)
    {
        perror("Error al asignar memoria");
        return NULL;
    }

    size_t bytesLeidos = fread(contenido, sizeof(char), longitudArchivo, archivo);
    if (bytesLeidos != longitudArchivo)
    {
        perror("Error al leer el archivo");
        free(contenido);
        return NULL;
    }

    contenido[longitudArchivo] = '\0'; // Asegurarse de que la cadena esté terminada
    return contenido;
}

// Función para escribir una cadena en un archivo
int escribirArchivo(FILE *archivo, const char *contenido)
{
    if (archivo == NULL)
    {
        return -1;
    }

    size_t longitud = fwrite(contenido, sizeof(char), strlen(contenido), archivo);
    if (longitud != strlen(contenido))
    {
        perror("Error al escribir en el archivo");
        return -1;
    }

    return 0;
}

// Función para cerrar un archivo
void cerrarArchivo(FILE *archivo)
{
    if (archivo != NULL)
    {
        fclose(archivo);
    }
}

char *convertirMinusculas(const char *cadena)
{
    // Obtener la longitud de la cadena original
    size_t longitud = strlen(cadena);

    // Reservar memoria para la nueva cadena en minúsculas
    char *cadenaLowerCase = (char *)malloc((longitud + 1) * sizeof(char)); // +1 para el terminador nulo

    if (cadenaLowerCase == NULL)
    {
        perror("Error al asignar memoria");
        return NULL;
    }

    // Convertir cada carácter a minúscula
    for (size_t i = 0; i < longitud; i++)
    {
        cadenaLowerCase[i] = tolower(cadena[i]);
    }

    // Añadir el terminador nulo al final de la nueva cadena
    cadenaLowerCase[longitud] = '\0';

    return cadenaLowerCase;
}