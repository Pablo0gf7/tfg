#include "../headers/utils.h"
#include <stdio.h>
#include <sndfile.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../headers/lsb.h"

/**
 * @file utils.c
 * @brief Utilidades de E/S y cálculo de métricas (MSE y PSNR) para audio usando libsndfile.
 *
 * @details
 *  Este archivo reúne funciones de apoyo para el TFG de esteganografía en audio:
 *   - Construcción de rutas de salida simples.
 *   - Cálculo de MSE y PSNR entre un audio original y su versión estego (hasta un número de muestras).
 *   - Lectura y escritura de ficheros de texto y binarios.
 *   - Conversión de cadenas a minúsculas.
 *
 */

/**
 * @brief Construye una ruta relativa "./out/filename" sobre un buffer estático.
 *
 * @param filename Nombre del fichero (sin ruta) que se desea ubicar dentro de ./out.
 * @return Puntero a un buffer estático con la ruta resultante. **No thread-safe**; 
 *         el contenido se sobrescribe en cada llamada.
 */
const char *build_path_static(const char *filename) {
    static char path[512];  
    snprintf(path, sizeof(path), "%s/%s", "./out", filename);
    return path;            
}

/**
 * @brief Calcula MSE y PSNR entre dos ficheros de audio y guarda los resultados en un .txt.
 *
 * Lee ambos ficheros con libsndfile en bloques de 1024 frames, compara muestra a muestra hasta
 * alcanzar `samples_to_check` (total de muestras teniendo en cuenta todos los canales) o el fin 
 * de los archivos. Asume que ambos ficheros tienen el mismo número de frames y canales.
 *
 * @param original_file Ruta al audio original.
 * @param stego_file    Ruta al audio con esteganografía (o procesado) a comparar.
 * @param output_txt    Ruta del fichero de salida (texto) donde se escribe MSE y PSNR.
 * @param samples_to_check Número máximo de muestras a comparar (incluye todos los canales).
 *
 */
void calculate_mse_psnr(const char *original_file, const char *stego_file, const char *output_txt, sf_count_t samples_to_check) {
    SF_INFO sfinfo1, sfinfo2;
    SNDFILE *f1 = sf_open(original_file, SFM_READ, &sfinfo1);
    SNDFILE *f2 = sf_open(stego_file, SFM_READ, &sfinfo2);

    // Comprobación de apertura de ficheros
    if (!f1 || !f2) {
        printf("Error opening files\n");
        if (f1) sf_close(f1);
        if (f2) sf_close(f2);
        return;
    }

    // Verificación de compatibilidad básica: mismo nº de frames y de canales
    if (sfinfo1.frames != sfinfo2.frames || sfinfo1.channels != sfinfo2.channels) {
        printf("Files must have the same length and channels\n");
        sf_close(f1); sf_close(f2);
        return;
    }

    float buf1[1024], buf2[1024];        // Buffers temporales para lectura en formato float
    sf_count_t total = 0;                // Número total de muestras comparadas (todas las de todos los canales)
    double mse = 0.0;                    // Suma acumulada de los cuadrados del error (se normaliza al final)
    sf_count_t read1, read2;             // Frames leídos en cada iteración
    sf_count_t samples_compared = 0;     // Muestras ya comparadas (cuenta por canal)
    int channels = sfinfo1.channels;     // Nº de canales del audio

    // Bucle de lectura por frames de 1024
    while ((read1 = sf_readf_float(f1, buf1, 1024)) > 0 &&
           (read2 = sf_readf_float(f2, buf2, 1024)) > 0 &&
           samples_compared < samples_to_check) {
        
        sf_count_t frames_to_process = read1; 

 
        if ((samples_compared + read1 * channels) > samples_to_check) {
            frames_to_process = (samples_to_check - samples_compared) / channels;
        }

        // Recorremos frame*canales como un stream lineal de muestras
        for (int i = 0; i < frames_to_process * channels; ++i) {
            double diff = buf1[i] - buf2[i];
            mse += diff * diff; 
            total++;            
        }

        samples_compared += frames_to_process * channels;

        if (samples_compared >= samples_to_check) {
            break; 
        }
    }


    mse /= total; 
    double psnr = 10.0 * log10(1.0 / mse); 

    // Escribir resultados a fichero de texto
    FILE *out = fopen(output_txt, "w");
    if (!out) {
        printf("Error opening output file\n");
    } else {
        fprintf(out, "MSE (modified part): %.8f\n", mse);
        fprintf(out, "PSNR (modified part): %.2f dB\n", psnr);
        fclose(out);
        printf("Results saved to %s\n", output_txt);
    }

    sf_close(f1);
    sf_close(f2);
}

/**
 * @brief Lee un fichero de texto completo y devuelve un buffer NUL-terminado.
 *
 * @param filename Ruta del fichero de texto.
 * @return Puntero a memoria dinámica con el contenido y terminador '\0'.
 *         Debe liberarse con `free()`. Devuelve NULL si hay error.
 */
char *read_text_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    printf("Intentando abrir: %s\n", filename);
    if (!f) {
        printf("Error opening file: %s\n", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);       
    rewind(f);
    char *buffer = (char*)malloc(len + 1);
    if (!buffer) {
        fclose(f);
        printf("Memory allocation failed\n");
        return NULL;
    }
    fread(buffer, 1, len, f);  
    buffer[len] = '\0';       
    fclose(f);
    return buffer;
}

/**
 * @brief Guarda un mensaje binario en disco tal cual (sin terminador).
 *
 * @param filename Ruta de salida.
 * @param msg      Puntero al buffer con los datos a escribir.
 * @param msg_len  Longitud en bytes a escribir.
 */
void save_message_to_file(const char *filename, const unsigned char *msg, size_t msg_len) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("Error abriendo fichero para guardar mensaje");
        return;
    }
    fwrite(msg, 1, msg_len, f);
    fclose(f);
}

// ============================
// Utilidades genéricas de E/S
// ============================

/**
 * @brief Abre un archivo con el modo indicado y devuelve su FILE*.
 *
 * @param nombreArchivo Ruta del archivo a abrir.
 * @param modo          Modo de apertura (por ejemplo, "rb", "wb", "r", etc.).
 * @return Puntero FILE* o NULL si falla (se habrá informado mediante perror).
 */
FILE *abrirArchivo(const char *nombreArchivo, const char *modo)
{
    FILE *archivo = fopen(nombreArchivo, modo);
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo");
    }
    return archivo;
}

/**
 * @brief Lee todo el contenido de un archivo abierto y lo devuelve en un buffer NUL-terminado.
 *
 * @param archivo Puntero FILE* ya abierto para lectura.
 * @return Cadena con el contenido (heap). Debe liberarse con `free()`. NULL si hay error.
 */
char *leerArchivo(FILE *archivo)
{
    if (archivo == NULL)
    {
        return NULL;
    }

    fseek(archivo, 0, SEEK_END);           
    long longitudArchivo = ftell(archivo);
    rewind(archivo);                       

    char *contenido = (char *)malloc((longitudArchivo + 1) * sizeof(char)); 
    if (contenido == NULL)
    {
        perror("Error al asignar memoria");
        return NULL;
    }

    size_t bytesLeidos = fread(contenido, sizeof(char), longitudArchivo, archivo);
    if (bytesLeidos != (size_t)longitudArchivo)
    {
        perror("Error al leer el archivo");
        free(contenido);
        return NULL;
    }

    contenido[longitudArchivo] = '\0'; 
    return contenido;
}

/**
 * @brief Escribe una cadena completa en un archivo abierto.
 *
 * @param archivo   Puntero FILE* abierto en modo escritura.
 * @param contenido Cadena C (terminada en '\0') a escribir.
 * @return 0 si OK, -1 si hay error (consultar errno/perror()).
 */
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

/**
 * @brief Cierra un archivo si el puntero no es NULL.
 */
void cerrarArchivo(FILE *archivo)
{
    if (archivo != NULL)
    {
        fclose(archivo);
    }
}

/**
 * @brief Devuelve una copia de la cadena de entrada convertida íntegramente a minúsculas.
 *
 * @param cadena Cadena de entrada (no se modifica).
 * @return Nueva cadena en minúsculas (heap). Debe liberarse con `free()`.
 */
char *convertirMinusculas(const char *cadena)
{
    
    size_t longitud = strlen(cadena);

    
    char *cadenaLowerCase = (char *)malloc((longitud + 1) * sizeof(char)); 

    if (cadenaLowerCase == NULL)
    {
        perror("Error al asignar memoria");
        return NULL;
    }

    // Convertir cada carácter a minúscula
    for (size_t i = 0; i < longitud; i++)
    {
        cadenaLowerCase[i] = (char)tolower((unsigned char)cadena[i]);
    }

    // Añadir el terminador nulo al final de la nueva cadena
    cadenaLowerCase[longitud] = '\0';

    return cadenaLowerCase;
}

/**
 * @brief Escribe un bloque de memoria arbitrario en un archivo abierto.
 *
 * @param archivo  Puntero FILE* (normalmente abierto con "wb").
 * @param datos    Puntero al buffer a escribir.
 * @param longitud Número de bytes a escribir.
 * @return 0 si OK, -1 si hay error.
 */
int escribirBuffer(FILE *archivo, const void *datos, size_t longitud)
{
    if (archivo == NULL || datos == NULL) return -1;
    size_t escritos = fwrite(datos, 1, longitud, archivo);
    if (escritos != longitud) { perror("fwrite"); return -1; }
    return 0;
}

/**
 * @brief Abre un archivo en modo binario y vuelca el buffer indicado.
 *
 * @param ruta     Ruta del archivo destino.
 * @param datos    Puntero al buffer a escribir.
 * @param longitud Número de bytes a escribir.
 * @return 0 si OK, -1 si error al abrir o escribir.
 */
int escribirArchivoBin(const char *ruta, const void *datos, size_t longitud)
{
    FILE *f = abrirArchivo(ruta, "wb");
    if (!f) return -1;
    int rc = escribirBuffer(f, datos, longitud);
    cerrarArchivo(f);
    return rc;
}
