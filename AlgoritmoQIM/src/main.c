#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/qim.h"
#include "../headers/utils.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }
    if (strcmp(argv[1], "embedfile") == 0 && argc == 5)
    {
        char *msg = read_text_file(argv[4]);
        if (!msg)
        {
            printf("No se pudo leer el archivo de mensaje.\n");
            return 1;
        }
        embed_message(argv[2], argv[3], msg);
        free(msg);
    }
    else if (strcmp(argv[1], "embed") == 0 && argc == 5)
    {
        embed_message(argv[2], argv[3], argv[4]);
    }
    else if (strcmp(argv[1], "extract") == 0 && argc == 4)
    {
        extract_message(argv[2], atoi(argv[3]));
    }
    else if (strcmp(argv[1], "metrics") == 0 && argc == 5)
    {
        // calculate_mse_psnr(argv[2], argv[3], argv[4]);
    }
    else
    {
        print_usage();
    }
    return 0;
}
//./algoritmoqim.exe embedfile audio/original.wav audio/oculto.wav ./messages/mensaje.txt
// gcc src/*.c -Iheaders -lsndfile -lfftw3 -lm -lssl -lcrypto -o algoritmoqim.exe
//./algoritmoqim.exe embed audio/pista2.wav audio/oculto.wav "hola esta es la prueba sin encriptar"
// ./algoritmoqim.exe extract audio/oculto.wav 4
// https://theremin.music.uiowa.edu/studentworks.html