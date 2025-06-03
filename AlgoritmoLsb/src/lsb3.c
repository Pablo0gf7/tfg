#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image_write.h"

#include "../headers/lsb3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include <stdint.h>

void embedMessage(const char *imagePath, const char *message, const char *outputPath, const char *key)
{
    if (sodium_init() < 0) {
        fprintf(stderr, "libsodium init failed.\n");
        return;
    }

    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) {
        fprintf(stderr, "Error loading image: %s\n", imagePath);
        return;
    }
unsigned char *img_original = malloc(width * height * channels);
memcpy(img_original, img, width * height * channels);
    size_t messageLen = strlen(message);
    size_t cipherLen = messageLen + crypto_secretbox_MACBYTES;

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof nonce);

    unsigned char key_bin[crypto_secretbox_KEYBYTES];
    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    unsigned char *cipher = malloc(cipherLen);
    crypto_secretbox_easy(cipher, (const unsigned char *)message, messageLen, nonce, key_bin);

    // Construir estructura final: "###" + nonce + msg_len + cipher_len + cipher + "###"
    uint32_t msg_len_le = (uint32_t)messageLen;
    uint32_t cipher_len_le = (uint32_t)cipherLen;

    size_t totalLen = 3 + crypto_secretbox_NONCEBYTES + 4 + 4 + cipherLen + 3;
    unsigned char *finalData = malloc(totalLen);
    unsigned char *ptr = finalData;

    memcpy(ptr, "###", 3); ptr += 3;
    memcpy(ptr, nonce, crypto_secretbox_NONCEBYTES); ptr += crypto_secretbox_NONCEBYTES;
    memcpy(ptr, &msg_len_le, 4); ptr += 4;
    memcpy(ptr, &cipher_len_le, 4); ptr += 4;
    memcpy(ptr, cipher, cipherLen); ptr += cipherLen;
    memcpy(ptr, "###", 3); 

    // Comprobar espacio
    size_t bitsNeeded = totalLen * 8;
    size_t bitsAvailable = width * height * 3;
    if (bitsNeeded > bitsAvailable) {
        fprintf(stderr, "Encrypted data too large for image.\n");
        free(cipher); free(finalData); stbi_image_free(img);
        return;
    }

    // Ocultar bits LSB
    size_t bitIndex = 0;
    for (size_t i = 0; i < totalLen; ++i) {
        for (int bit = 7; bit >= 0; --bit) {
            unsigned char bitValue = (finalData[i] >> bit) & 1;
            size_t pixel = bitIndex / 3;
            size_t color = bitIndex % 3;
            img[pixel * channels + color] = (img[pixel * channels + color] & ~1) | bitValue;
            bitIndex++;
        }
    }
    if (!stbi_write_png(build_path_static(outputPath), width, height, channels, img, width * channels)) {
        fprintf(stderr, "Error saving image to %s\n", outputPath);
    }
    calculate_and_save_mse_psnr(img_original, img, width, height, channels, build_path_static("results.txt"));
    free(cipher);
    free(finalData);
    stbi_image_free(img);
}



char *extractMessage(const char *imagePath, const char *key)
{
    if (sodium_init() < 0) {
        fprintf(stderr, "libsodium init failed.\n");
        return NULL;
    }

    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) {
        fprintf(stderr, "Error loading image: %s\n", imagePath);
        return NULL;
    }

    size_t maxBits = width * height * 3;
    size_t maxBytes = maxBits / 8;
    unsigned char *data = malloc(maxBytes);

    // Extraer todos los bits LSB
    size_t bitIndex = 0;
    for (size_t i = 0; i < maxBytes; ++i) {
        unsigned char byte = 0;
        for (int bit = 7; bit >= 0; --bit) {
            size_t pixel = bitIndex / 3;
            size_t color = bitIndex % 3;
            byte |= ((img[pixel * channels + color] & 1) << bit);
            bitIndex++;
        }
        data[i] = byte;
    }

    // Buscar "###" al inicio
    if (memcmp(data, "###", 3) != 0) {
        fprintf(stderr, "No se encontró el marcador inicial.\n");
        free(data); stbi_image_free(img);
        return NULL;
    }

    unsigned char *ptr = data + 3;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, ptr, crypto_secretbox_NONCEBYTES); ptr += crypto_secretbox_NONCEBYTES;

    uint32_t msg_len, cipher_len;
    memcpy(&msg_len, ptr, 4); ptr += 4;
    memcpy(&cipher_len, ptr, 4); ptr += 4;

    if (cipher_len > maxBytes - (ptr - data) - 3) {
        fprintf(stderr, "Cipher length is too large or corrupted.\n");
        free(data); stbi_image_free(img);
        return NULL;
    }

    unsigned char *cipher = malloc(cipher_len);
    memcpy(cipher, ptr, cipher_len); ptr += cipher_len;

    if (memcmp(ptr, "###", 3) != 0) {
        fprintf(stderr, "No se encontró el marcador final.\n");
        free(data); free(cipher); stbi_image_free(img);
        return NULL;
    }

    unsigned char key_bin[crypto_secretbox_KEYBYTES];
    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    unsigned char *decrypted = malloc(cipher_len);
    if (crypto_secretbox_open_easy(decrypted, cipher, cipher_len, nonce, key_bin) != 0) {
        fprintf(stderr, "Decryption failed.\n");
        free(decrypted); free(data); free(cipher); stbi_image_free(img);
        return NULL;
    }

    decrypted[msg_len] = '\0';  // Asegura null terminación

    free(data);
    free(cipher);
    stbi_image_free(img);
    return (char *)decrypted;
}




// img1 y img2 deben tener el mismo tamaño y número de canales
// width, height: dimensiones
// channels: normalmente 3 para RGB
void calculate_and_save_mse_psnr(const unsigned char *img1, const unsigned char *img2,
                                int width, int height, int channels,
                                const char *output_file) {
    double mse[3] = {0.0, 0.0, 0.0};
    double mse_total = 0.0;
    int num_pixels = width * height;

    for (int i = 0; i < num_pixels; i++) {
        for (int c = 0; c < channels; c++) {
            int idx = i * channels + c;
            double diff = (double)img1[idx] - (double)img2[idx];
            mse[c] += diff * diff;
        }
    }

    for (int c = 0; c < channels; c++) {
        mse[c] /= num_pixels;
        mse_total += mse[c];
    }
    mse_total /= channels;

    double psnr[3];
    double psnr_total;
    const double MAX_PIXEL = 255.0;

    for (int c = 0; c < channels; c++) {
        if (mse[c] == 0) {
            psnr[c] = INFINITY; // No error
        } else {
            psnr[c] = 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse[c]);
        }
    }
    if (mse_total == 0) {
        psnr_total = INFINITY;
    } else {
        psnr_total = 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_total);
    }

    FILE *f = fopen(output_file, "w");
    if (!f) {
        perror("Error opening file");
        return;
    }

    fprintf(f, "MSE (R): %.6f\n", mse[0]);
    fprintf(f, "MSE (G): %.6f\n", mse[1]);
    fprintf(f, "MSE (B): %.6f\n", mse[2]);
    fprintf(f, "MSE (Average): %.6f\n\n", mse_total);

    fprintf(f, "PSNR (R): %.6f dB\n", psnr[0]);
    fprintf(f, "PSNR (G): %.6f dB\n", psnr[1]);
    fprintf(f, "PSNR (B): %.6f dB\n", psnr[2]);
    fprintf(f, "PSNR (Average): %.6f dB\n", psnr_total);

    fclose(f);
}


