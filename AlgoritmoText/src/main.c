#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "funcionesAux.h"
#include "utils.h"
#include <ctype.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s [-c|-e] [archivo1] [archivo2]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0)
    {
        if (argc != 4)
        {
            printf("Error: Debes proporcionar 2 archivos con la opción -c\n");
            return 1;
        }

        // Aquí puedes añadir la lógica para la opción -c
        printf("Modo ocultacion del mensaje seleccionado. Archivo del mensaje: %s, Archivo en el que se ocultara: %s\n", argv[2], argv[3]);
        //./binario -c mensaje.txt texto1000pal.txt

        const char *nombreArchivo_original = argv[2];
        // Abre el archivo usando la función abrirArchivo de utils
        FILE *archivo1 = abrirArchivo(nombreArchivo_original, "r");
        if (archivo1 == NULL)
        {
            printf("No se pudo abrir el archivo.\n");
            return 1;
        }
        // Lee el contenido completo del archivo usando leerArchivo
        char *mensaje = leerArchivo(archivo1);
        if (mensaje == NULL)
        {
            printf("Error al leer el archivo.\n");
            return 1;
        }
        char *mensajeBase16 = codificarMsg(mensaje);
        free(mensaje);
        // printf("Este es el mensaje codificado \n");
        imprimirCadena(mensajeBase16);
        cerrarArchivo(archivo1); // Cierra el archivo después de leer
        FILE *archivo2 = abrirArchivo(argv[3], "r");
        if (archivo2 == NULL)
        {
            printf("No se pudo abrir el archivo.\n");
            return 1;
        }
        char *contenidoArchivo = leerArchivo(archivo2);
        if (contenidoArchivo == NULL)
        {
            printf("Error al leer el archivo.\n");
            return 1;
        }
        char *textLowerCase = convertirMinusculas(contenidoArchivo);
        cerrarArchivo(archivo2); // Cierra el archivo después de leer
        int j = 0, fin = 0;
        for (int i = 0; i < strlen(textLowerCase); i++)
        {
            if (tolower(mensajeBase16[j]) == textLowerCase[i] && !fin)
            {
                textLowerCase[i] = toupper(textLowerCase[i]);
                // TODO debo poner una comprobacion si se ha llegado al final del mensaje y acabado o el for o no para mandar un error en caso de que no se haya podido encubrir ltodo el mensaje
                if (mensajeBase16[j] == '\0')
                    fin = 1;
                j++;
            }
            /**
             * ? Quizas añadir que si se supera el maximo de caracteres primero realzia runa conversion a base64 del payload que con un
             * ? texto de aprox 1500 palabras cabe perfectamente como esta el de ejemplo
             */
            /*
            if(!fin){
                fprintf(stderr, "Error el mensaje que se debe encubrir es demasiado largo para el texto proporcionado.\n");
                return 1;
            }
            */
        }
        FILE *resultado = abrirArchivo("./steganogram.txt", "w");
        escribirArchivo(resultado, textLowerCase);
        cerrarArchivo(resultado);
        printf("\n");
    }
    else if (strcmp(argv[1], "-e") == 0)
    {
        if (argc != 3)
        {
            printf("Error: Debes proporcionar 1 archivo con la opción -e\n");
            return 1;
        }

        // Aquí puedes añadir la lógica para la opción -e
        printf("Modo extraccion del mensaje seleccionado. Archivo: %s\n", argv[2]);
        //./binario -e esteganograma.txt

        // Extraccion del mensaje de un archivo
        // DECODIFICACION DEL MENSAJE
        FILE *objetivo = abrirArchivo(argv[2], "r");
        char *text_cubierto = leerArchivo(objetivo);
        imprimirCadena(decodificarMsg(extraerMayusculas(text_cubierto)));
    }
    else
    {
        // Si el argumento no es ni -d ni -e, mostramos un mensaje de error
        printf("Opción no reconocida debe ser [-c|-e]: %s\n", argv[1]);
        return 1;
    }
    return 0;
}
