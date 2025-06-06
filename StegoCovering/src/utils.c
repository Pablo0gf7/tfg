#include "../headers/utils.h"
#include <stdio.h>
#include <sndfile.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../headers/lsb3.h"

const char *build_path_static( const char *filename) {
    static char path[512];  
    snprintf(path, sizeof(path), "%s/%s", "./out", filename);
    return path;  
}

void printUsage()
{
    printf("Usage:\n");
    printf("  Interactive mode: ./executable -i\n");
    printf("  To embed a message: ./executable -c <image_path> <message> <output_path> <key>\n");
    printf("  To extract a message: ./executable -e <image_path> <key>\n");
}

void interactiveMode()
{
    int choice;
    char imagePath[256];
    char message[1024];
    char outputPath[256];
    char key[256];

    printf("Welcome to the interactive mode.\n");
    printf("Choose an option:\n");
    printf("  1. Embed a message\n");
    printf("  2. Extract a message\n");
    printf("Select an option (1 or 2): ");
    scanf("%d", &choice);
    getchar(); 

    if (choice == 1)
    {
        printf("Enter image path: ");
        fgets(imagePath, sizeof(imagePath), stdin);
        imagePath[strcspn(imagePath, "\n")] = 0; 

        printf("Enter message to embed: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        printf("Enter output image path: ");
        fgets(outputPath, sizeof(outputPath), stdin);
        outputPath[strcspn(outputPath, "\n")] = 0;

        printf("Enter encryption key: ");
        fgets(key, sizeof(key), stdin);
        key[strcspn(key, "\n")] = 0;

        embedMessage(imagePath, message, outputPath, key);
    }
    else if (choice == 2)
    {
        printf("Enter image path: ");
        fgets(imagePath, sizeof(imagePath), stdin);
        imagePath[strcspn(imagePath, "\n")] = 0;

        printf("Enter decryption key: ");
        fgets(key, sizeof(key), stdin);
        key[strcspn(key, "\n")] = 0;

        char *decryptedMessage = extractMessage(imagePath, key);
        if (decryptedMessage)
        {
            printf("Extracted message: %s\n", decryptedMessage);
            free(decryptedMessage);
        }
    }
    else
    {
        printf("Invalid choice.\n");
    }
}

void print_usage() {
    printf("Usage:\n");
    printf("  ./algoritmoqim embed <in.wav> <out.wav> <message>\n");
    printf("  ./algoritmoqim embedfile <in.wav> <out.wav> <message.txt>\n");
    printf("  ./algoritmoqim extract <in.wav> <msg_len>\n");
    printf("  ./algoritmoqim metrics <original.wav> <stego.wav> <output.txt>\n");
}

void calculate_mse_psnr(const char *original_file, const char *stego_file, const char *output_txt, sf_count_t samples_to_check) {
    SF_INFO sfinfo1, sfinfo2;
    SNDFILE *f1 = sf_open(original_file, SFM_READ, &sfinfo1);
    SNDFILE *f2 = sf_open(stego_file, SFM_READ, &sfinfo2);

    if (!f1 || !f2) {
        printf("Error opening files\n");
        if (f1) sf_close(f1);
        if (f2) sf_close(f2);
        return;
    }

    if (sfinfo1.frames != sfinfo2.frames || sfinfo1.channels != sfinfo2.channels) {
        printf("Files must have the same length and channels\n");
        sf_close(f1); sf_close(f2);
        return;
    }

    float buf1[1024], buf2[1024];
    sf_count_t total = 0;
    double mse = 0.0;
    sf_count_t read1, read2;
    sf_count_t samples_compared = 0;
    int channels = sfinfo1.channels;

    while ((read1 = sf_readf_float(f1, buf1, 1024)) > 0 &&
           (read2 = sf_readf_float(f2, buf2, 1024)) > 0 &&
           samples_compared < samples_to_check) {
        
        sf_count_t frames_to_process = read1;
        // No procesar más muestras que el límite
        if ((samples_compared + read1 * channels) > samples_to_check) {
            frames_to_process = (samples_to_check - samples_compared) / channels;
        }

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

void save_message_to_file(const char *filename, const unsigned char *msg, size_t msg_len) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("Error abriendo fichero para guardar mensaje");
        return;
    }
    fwrite(msg, 1, msg_len, f);
    fclose(f);
}

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