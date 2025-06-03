#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/lsb3.h"


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    if (strcmp(argv[1], "-i") == 0)
    {
        interactiveMode();
    }
    else if (strcmp(argv[1], "-c") == 0)
    {
        if (argc != 6)
        {
            printUsage();
            return 1;
        }
        const char *imagePath = argv[2];
        const char *message = argv[3];
        const char *outputPath = argv[4];
        const char *key = argv[5];
        embedMessage(imagePath, message, outputPath, key);
    }
    else if (strcmp(argv[1], "-e") == 0)
    {
        if (argc != 4)
        {
            printUsage();
            return 1;
        }
        const char *imagePath = argv[2];
        const char *key = argv[3];
        char *decryptedMessage = extractMessage(imagePath, key);
        if (decryptedMessage)
        {
            printf("Extracted message: %s\n", decryptedMessage);
            free(decryptedMessage);
        }
    }
    else
    {
        printUsage();
        return 1;
    }

    return 0;
}
