#define _GNU_SOURCE
#include "../headers/qim.h"      // Header propio con prototipos
#include <stdio.h>               // Entrada/salida estándar
#include <stdlib.h>              // Funciones de utilidad (malloc, free, etc.)
#include <string.h>              // Manejo de cadenas
#include <sndfile.h>             // Librería para leer/escribir audio
#include <fftw3.h>               // Librería para DCT/FFT
#include <math.h>    
#include <utils.h>   
         // Funciones matemáticas (round, etc.)

#define BLOCK_SIZE 1024          // Tamaño de bloque para procesar el audio
#define DELTA 0.2              // Parámetro de cuantización QIM

// Función auxiliar: inserta un bit en un coeficiente usando QIM
static double qim_embed(double coef, int bit, double delta) {
    double q = round(coef / delta);           // Cuantiza el coeficiente
    if (((int)q % 2) != bit)                  // Si el bit no coincide, ajusta la paridad
        q += (bit - ((int)q % 2));
    return q * delta;                         // Devuelve el coeficiente modificado
}

// Función auxiliar: extrae un bit de un coeficiente usando QIM
static int qim_extract(double coef, double delta) {
    int q = (int)round(coef / delta);         // Cuantiza el coeficiente
    return q % 2;                             // Devuelve el bit extraído (paridad)
}

// Función principal para ocultar un mensaje en un archivo de audio
void embed_message(const char *infile, const char *outfile, const char *message) {
    SF_INFO sfinfo;                           // Estructura con info del archivo de audio
    SNDFILE *in = sf_open(infile, SFM_READ, &sfinfo); // Abre archivo de entrada
    if (!in) {
        fprintf(stderr, "Error opening input file\n");
        return;
    }

    SNDFILE *out = sf_open(outfile, SFM_WRITE, &sfinfo); // Abre archivo de salida
    if (!out) {
        fprintf(stderr, "Error opening output file\n");
        sf_close(in);
        return;
    }

char *msg_with_end = NULL;
asprintf(&msg_with_end, "%s###", message); // Añade marcador de fin
int msg_len = (int)strlen(msg_with_end) * 8;

    int msg_pos = 0;                          // Posición actual en el mensaje (en bits)

    // Reserva memoria para el mensaje en bits
    uint8_t *bits = (uint8_t*)malloc(msg_len * sizeof(uint8_t));
    if (!bits) {
        fprintf(stderr, "Memory allocation failed\n");
        sf_close(in);
        sf_close(out);
        return;
    }

    // Convierte el mensaje de texto a bits (MSB primero)
    for (size_t i = 0; i < strlen(msg_with_end); ++i) {
    for (int b = 7; b >= 0; --b) {
        bits[msg_pos++] = (msg_with_end[i] >> b) & 1;
    }
}
    msg_pos = 0; // Reinicia posición para el proceso de embedding

    float buffer[BLOCK_SIZE];     // Buffer para leer audio en float
    double dbuffer[BLOCK_SIZE];   // Buffer para procesar con FFTW (double)

    int frames;
    // Procesa el audio en bloques
    while ((frames = sf_readf_float(in, buffer, BLOCK_SIZE)) > 0) {
        // Copia el buffer de float a double para FFTW
        for (int i = 0; i < frames; ++i)
            dbuffer[i] = buffer[i];

        // Crea plan de DCT (tipo II) para el bloque
        fftw_plan plan = fftw_plan_r2r_1d(frames, dbuffer, dbuffer, FFTW_REDFT10, FFTW_ESTIMATE);
        if (!plan) {
            fprintf(stderr, "FFTW plan creation failed\n");
            free(bits);
            sf_close(in);
            sf_close(out);
            return;
        }
        fftw_execute(plan); // Ejecuta la DCT

        // Inserta los bits del mensaje en los coeficientes DCT (evita el primero)
        for (int i = 1; i < frames && msg_pos < msg_len; ++i, ++msg_pos) {
            dbuffer[i] = qim_embed(dbuffer[i], bits[msg_pos], DELTA);
        }

        // Crea plan de DCT inversa (tipo III)
        fftw_plan iplan = fftw_plan_r2r_1d(frames, dbuffer, dbuffer, FFTW_REDFT01, FFTW_ESTIMATE);
        if (!iplan) {
            fprintf(stderr, "FFTW inverse plan creation failed\n");
            fftw_destroy_plan(plan);
            free(bits);
            sf_close(in);
            sf_close(out);
            return;
        }
        fftw_execute(iplan); // Ejecuta la DCT inversa

        // Normaliza el resultado tras la inversa (por la definición de FFTW_REDFT01)
        for (int i = 0; i < frames; ++i)
            buffer[i] = (float)(dbuffer[i] / (2 * frames));

        // Escribe el bloque modificado en el archivo de salida
        sf_writef_float(out, buffer, frames);

        // Libera los planes de FFTW
        fftw_destroy_plan(plan);
        fftw_destroy_plan(iplan);
    }
free(msg_with_end);

    free(bits);         // Libera memoria del mensaje en bits
    sf_close(in);       // Cierra archivo de entrada
    sf_close(out);      // Cierra archivo de salida
    printf("Message embedded!\n"); // Mensaje de éxito

     calculate_mse_psnr(infile, outfile, "./out/resultados.txt",msg_len);
}

// Función principal para extraer un mensaje oculto de un archivo de audio
void extract_message(const char *infile, int msg_bytes) {
    SF_INFO sfinfo;                             // Estructura con info del archivo de audio
    SNDFILE *in = sf_open(infile, SFM_READ, &sfinfo); // Abre archivo de entrada
    if (!in) {
        fprintf(stderr, "Error opening input file\n");
        return;
    }

    int msg_len = msg_bytes * 8;                // Longitud del mensaje en bits
    int msg_pos = 0;                            // Posición actual en el mensaje (en bits)

    // Reserva memoria para los bits extraídos
    uint8_t *bits = (uint8_t*)malloc(msg_len * sizeof(uint8_t));
    if (!bits) {
        fprintf(stderr, "Memory allocation failed\n");
        sf_close(in);
        return;
    }

    float buffer[BLOCK_SIZE];                   // Buffer para leer audio en float
    double dbuffer[BLOCK_SIZE];                 // Buffer para procesar con FFTW (double)

    int frames;
    // Procesa el audio en bloques
    while ((frames = sf_readf_float(in, buffer, BLOCK_SIZE)) > 0 && msg_pos < msg_len) {
        // Copia el buffer de float a double para FFTW
        for (int i = 0; i < frames; ++i)
            dbuffer[i] = buffer[i];

        // Crea plan de DCT (tipo II) para el bloque
        fftw_plan plan = fftw_plan_r2r_1d(frames, dbuffer, dbuffer, FFTW_REDFT10, FFTW_ESTIMATE);
        if (!plan) {
            fprintf(stderr, "FFTW plan creation failed\n");
            free(bits);
            sf_close(in);
            return;
        }
        fftw_execute(plan); // Ejecuta la DCT

        // Extrae los bits de los coeficientes DCT (evita el primero)
        for (int i = 1; i < frames && msg_pos < msg_len; ++i, ++msg_pos) {
            bits[msg_pos] = qim_extract(dbuffer[i], DELTA);
        }

        fftw_destroy_plan(plan); // Libera el plan de FFTW
    }

    // Reconstruye el mensaje hasta encontrar "###"
printf("Extracted message: ");
char c = 0;
int bit_count = 0;
char end_check[4] = {0};

for (int i = 0; i < msg_pos; ++i) {
    c = (c << 1) | bits[i];
    bit_count++;

    if (bit_count == 8) {
        bit_count = 0;
        printf("%c", c);

        // Desplaza ventana de 3 últimos caracteres
        end_check[0] = end_check[1];
        end_check[1] = end_check[2];
        end_check[2] = c;
        end_check[3] = '\0';

        if (strcmp(end_check, "###") == 0) break;
        c = 0;
    }
}
printf("\n");


    free(bits);     // Libera memoria de los bits
    sf_close(in);   // Cierra archivo de entrada
}