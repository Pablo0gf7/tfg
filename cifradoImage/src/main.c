#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/lsb3.h"

void printUsage()
{
    printf("Usage:\n");
    printf("  Interactive mode: ./executable -i\n");
    printf("  To embed a message: ./executable embed <image_path> <message> <output_path> <key>\n");
    printf("  To extract a message: ./executable extract <image_path> <key>\n");
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
    else if (strcmp(argv[1], "embed") == 0)
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
    else if (strcmp(argv[1], "extract") == 0)
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
