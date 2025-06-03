#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lsb3.h"

void printUsage() {
    printf("Usage:\n");
    printf("  To embed a message: ./lsb3-steganography embed <image_path> <message> <output_path>\n");
    printf("  To extract a message: ./lsb3-steganography extract <image_path> \n");
}

int main(int argc, char *argv[]) {
    if (argc < 2 ) {
        printUsage();
        return 1;
    }

    if (strcmp(argv[1], "embed") == 0) {
        if (argc != 5) {
            printUsage();
            return 1;
        }
        const char *imagePath = argv[2];
        const char *message = argv[3];
        const char *outputPath = argv[4];
        embedMessage(imagePath, message, outputPath);
    } else if (strcmp(argv[1], "extract") == 0) {
        if (argc != 3) {
            printUsage();
            return 1;
        }
        const char *imagePath = argv[2];
        size_t messageLength = (size_t)atoi(argv[3]);
        char *extractedMessage = extractMessage(imagePath);
        if (extractedMessage) {
            printf("Extracted message: %s\n", extractedMessage);
            free(extractedMessage);
        }
    } else {
        printUsage();
        return 1;
    }

    return 0;
}