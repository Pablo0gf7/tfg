#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../headers/utils.h"

// Textual
#include "../headers/funcionesAux.h"

// Imagen
#include "../headers/lsb2-random.h"

// Audio
#include "../headers/qim.h"

/* 
./steganografia -i -c imagenes/imagen.png "Hola mundo" imagenes/salida.png clave123
./steganografia -t -e textos/steganogram.txt
./steganografia -a embedfile audio/original.wav audio/oculto.wav messages/mensaje.txt
 */

void print_general_usage() {
    printf("Uso general:\n");
    printf("  Texto:\n");
    printf("    ./prog -t -c <mensaje.txt> <textoBase.txt>\n");
    printf("    ./prog -t -e <esteganograma.txt>\n\n");
    printf("  Imagen:\n");
    printf("    ./prog -i -c <imagen.png> <mensaje> <salida.png> <clave>\n");
    printf("    ./prog -i -e <imagen.png> <clave>\n\n");
    printf("  Audio:\n");
    printf("    ./prog -a embedfile <audio.wav> <salida.wav> <mensaje.txt>\n");
    printf("    ./prog -a embed <audio.wav> <salida.wav> <mensaje>\n");
    printf("    ./prog -a extract <audio.wav> <tamaño>\n");
    printf("    ./prog -a metrics <original.wav> <modificado.wav> <tamaño>\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_general_usage();
        return 1;
    }

    if (strcmp(argv[1], "-t") == 0)
    {
        // Esteganografía textual
        if (argc < 3) {
            print_general_usage();
            return 1;
        }
        if (strcmp(argv[2], "-c") == 0 && argc == 5)
        {
            FILE *fmsg = abrirArchivo(argv[3], "rb");
            char *msg = leerArchivo(fmsg);
            cerrarArchivo(fmsg);
            char *msgBase16 = codificarMsg(msg);
            free(msg);

            FILE *ftexto = abrirArchivo(argv[4], "rb");
            char *texto = leerArchivo(ftexto);
            cerrarArchivo(ftexto);
            char *textoMinus = convertirMinusculas(texto);
            free(texto);

            int j = 0;
            for (int i = 0; i < strlen(textoMinus) && msgBase16[j] != '\0'; i++)
            {
                if (tolower(msgBase16[j]) == textoMinus[i])
                {
                    textoMinus[i] = toupper(textoMinus[i]);
                    j++;
                }
            }

            if (msgBase16[j] != '\0') {
                fprintf(stderr, "El texto base no es suficientemente largo para ocultar el mensaje.\n");
                free(textoMinus);
                free(msgBase16);
                return 1;
            }

            FILE *fsalida = abrirArchivo("steganogram.txt", "w");
            escribirArchivo(fsalida, textoMinus);
            cerrarArchivo(fsalida);
            free(textoMinus);
            free(msgBase16);
        }
        else if (strcmp(argv[2], "-e") == 0 && argc == 4)
        {
            FILE *archivo = abrirArchivo(argv[3], "rb");
            char *contenido = leerArchivo(archivo);
            cerrarArchivo(archivo);
            char *extraido = extraerMayusculas(contenido);
            printf("Mensaje extraído: %s\n", decodificarMsg(extraido));
            free(contenido);
            free(extraido);
        }
        else
        {
            print_general_usage();
        }
    }
    else if (strcmp(argv[1], "-i") == 0)
    {
        // Esteganografía en imágenes
        if (strcmp(argv[2], "-c") == 0 && argc == 7)
        {
            embedMessage(argv[3], argv[4], argv[5], argv[6]);
        }
        else if (strcmp(argv[2], "-e") == 0 && argc == 5)
        {
            char *mensaje = extractMessage(argv[3], argv[4]);
            if (mensaje)
            {
                printf("Mensaje extraído: %s\n", mensaje);
                free(mensaje);
            }
        }
        else
        {
            printUsage(); // De lsb3.h
        }
    }
    else if (strcmp(argv[1], "-a") == 0)
    {
        // Esteganografía en audio
        if (strcmp(argv[2], "embedfile") == 0 && argc == 6)
        {
            char *msg = read_text_file(argv[5]);
            if (!msg) {
                printf("No se pudo leer el archivo del mensaje.\n");
                return 1;
            }
            embed_message(argv[3], argv[4], msg);
            free(msg);
        }
        else if (strcmp(argv[2], "embed") == 0 && argc == 6)
        {
            embed_message(argv[3], argv[4], argv[5]);
        }
        else if (strcmp(argv[2], "extract") == 0 && argc == 5)
        {
            extract_message(argv[3], atoi(argv[4]));
        }
        else if (strcmp(argv[2], "metrics") == 0 && argc == 6)
        {
            // calculate_mse_psnr(argv[3], argv[4], argv[5]);
            printf("MSE y PSNR aún no implementados.\n");
        }
        else
        {
            print_usage(); // De qim.h
        }
    }
    else
    {
        print_general_usage();
    }

    return 0;
}
