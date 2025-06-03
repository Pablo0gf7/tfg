#include "../headers/utils.h"
#include <stdio.h>


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

