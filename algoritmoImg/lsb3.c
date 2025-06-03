#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"

#include "lsb3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void embedMessage(const char *imagePath, const char *message, const char *outputPath)
{
    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);

    if (!img)
    {
        fprintf(stderr, "Error loading image: %s\n", imagePath);
        return;
    }

    if (channels < 3)
    {
        fprintf(stderr, "Image must have at least 3 channels (RGB).\n");
        stbi_image_free(img);
        return;
    }

    const char *delimiter = "###";
    size_t msgLen = strlen(message) + strlen(delimiter);
    char *messageWithEnd = malloc(msgLen + 1);
    strcpy(messageWithEnd, message);
    strcat(messageWithEnd, delimiter);

    size_t bitsNeeded = msgLen * 8;
    size_t pixelsAvailable = width * height;
    size_t bitsAvailable = pixelsAvailable * 3;

    if (bitsNeeded > bitsAvailable)
    {
        fprintf(stderr, "Message too long for this image.\n");
        stbi_image_free(img);
        return;
    }

    size_t bitIndex = 0;
    for (size_t i = 0; i < msgLen; ++i)
    {
        for (int bit = 7; bit >= 0; --bit)
        {
            unsigned char bitValue = (messageWithEnd[i] >> bit) & 1;
            size_t pixel = bitIndex / 3;
            size_t color = bitIndex % 3;

            img[pixel * channels + color] = (img[pixel * channels + color] & ~1) | bitValue;
            bitIndex++;
        }
    }
    free(messageWithEnd);

    if (!stbi_write_png(outputPath, width, height, channels, img, width * channels))
    {
        fprintf(stderr, "Error saving image to %s\n", outputPath);
    }

    stbi_image_free(img);
}

char *extractMessage(const char *imagePath) {
    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);

    if (!img) {
        fprintf(stderr, "Error loading image: %s\n", imagePath);
        return NULL;
    }

    if (channels < 3) {
        fprintf(stderr, "Image must have at least 3 channels (RGB).\n");
        stbi_image_free(img);
        return NULL;
    }

    size_t pixelsAvailable = width * height;
    size_t maxBits = pixelsAvailable * 3;
    size_t maxBytes = maxBits / 8;

    char *message = (char *)malloc(maxBytes + 1);
    if (!message) {
        fprintf(stderr, "Memory allocation error.\n");
        stbi_image_free(img);
        return NULL;
    }

    size_t bitIndex = 0, charIndex = 0;
    while (bitIndex + 8 <= maxBits) {
        unsigned char character = 0;
        for (int bit = 7; bit >= 0; --bit) {
            size_t pixel = bitIndex / 3;
            size_t color = bitIndex % 3;
            unsigned char bitValue = img[pixel * channels + color] & 1;
            character |= (bitValue << bit);
            bitIndex++;
        }
        message[charIndex++] = character;
        message[charIndex] = '\0';

        // Verifica si encontró el delimitador
        if (charIndex >= 3 && strcmp(&message[charIndex - 3], "###") == 0) {
            message[charIndex - 3] = '\0';  // corta el mensaje antes del delimitador
            break;
        }
    }

    stbi_image_free(img);
    return message;
}

