#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image_write.h"

#include "../headers/lsb2-random.h"
#include "../headers/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include <stdint.h>


void embedMessage(const char *imagePath, const char *message, const char *outputPath, const char *key) {
    if (sodium_init() < 0) return;

    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) return;

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

    size_t bitsNeeded = totalLen * 8;
    size_t totalPositions = width * height * 3;
    if (bitsNeeded > totalPositions) {
        fprintf(stderr, "Message too large.\n");
        free(cipher); free(finalData); stbi_image_free(img); free(img_original);
        return;
    }

    size_t *positions = malloc(totalPositions * sizeof(size_t));
    for (size_t i = 0; i < totalPositions; i++) positions[i] = i;

    unsigned char seed_hash[crypto_generichash_BYTES];
    crypto_generichash(seed_hash, sizeof seed_hash, (const unsigned char *)key, strlen(key), NULL, 0);
    srand(*(uint32_t *)seed_hash);

    for (size_t i = totalPositions - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        size_t tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }

    for (size_t bitIndex = 0; bitIndex < bitsNeeded; bitIndex++) {
        size_t bytePos = bitIndex / 8;
        int bit = 7 - (bitIndex % 8);
        unsigned char bitVal = (finalData[bytePos] >> bit) & 1;
        size_t pos = positions[bitIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        img[pixel * channels + color] = (img[pixel * channels + color] & ~1) | bitVal;
    }

    stbi_write_png(build_path_static(outputPath), width, height, channels, img, width * channels);

    calculate_and_save_mse_psnr(img_original, img, width, height, channels, "results.txt");
    calculate_and_save_mse_psnr_modified_only(img_original, img, width, height, 3);

    free(cipher); free(finalData); free(positions); stbi_image_free(img); free(img_original);
}

char *extractMessage(const char *imagePath, const char *key) {
    if (sodium_init() < 0) return NULL;

    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) return NULL;

    size_t totalPositions = width * height * 3;
    size_t *positions = malloc(totalPositions * sizeof(size_t));
    for (size_t i = 0; i < totalPositions; i++) positions[i] = i;

    unsigned char seed_hash[crypto_generichash_BYTES];
    crypto_generichash(seed_hash, sizeof seed_hash, (const unsigned char *)key, strlen(key), NULL, 0);
    srand(*(uint32_t *)seed_hash);

    for (size_t i = totalPositions - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        size_t tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }

    size_t maxHeaderBits = (3 + crypto_secretbox_NONCEBYTES + 4 + 4) * 8;
    unsigned char headerBuf[64] = {0};

    for (size_t bitIndex = 0; bitIndex < maxHeaderBits; ++bitIndex) {
        size_t bytePos = bitIndex / 8;
        int bit = 7 - (bitIndex % 8);
        size_t pos = positions[bitIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        headerBuf[bytePos] |= ((img[pixel * channels + color] & 1) << bit);
    }

    if (memcmp(headerBuf, "###", 3) != 0) {
        fprintf(stderr, "No initial marker found.\n");
        free(positions); stbi_image_free(img);
        return NULL;
    }

    unsigned char *ptr = headerBuf + 3;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, ptr, crypto_secretbox_NONCEBYTES); ptr += crypto_secretbox_NONCEBYTES;

    uint32_t msg_len, cipher_len;
    memcpy(&msg_len, ptr, 4); ptr += 4;
    memcpy(&cipher_len, ptr, 4); ptr += 4;

    size_t totalLen = 3 + crypto_secretbox_NONCEBYTES + 4 + 4 + cipher_len + 3;
    size_t bitsToExtract = totalLen * 8;
    unsigned char *data = calloc(totalLen, 1);

    for (size_t bitIndex = 0; bitIndex < bitsToExtract; ++bitIndex) {
        size_t bytePos = bitIndex / 8;
        int bit = 7 - (bitIndex % 8);
        size_t pos = positions[bitIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        data[bytePos] |= ((img[pixel * channels + color] & 1) << bit);
    }

    if (memcmp(data + totalLen - 3, "###", 3) != 0) {
        fprintf(stderr, "No final marker found.\n");
        free(positions); stbi_image_free(img); free(data);
        return NULL;
    }

    unsigned char *cipher = data + 3 + crypto_secretbox_NONCEBYTES + 4 + 4;
    unsigned char key_bin[crypto_secretbox_KEYBYTES];
    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    unsigned char *decrypted = malloc(msg_len + 1);
    if (crypto_secretbox_open_easy(decrypted, cipher, cipher_len, nonce, key_bin) != 0) {
        fprintf(stderr, "Decryption failed.\n");
        free(decrypted); free(data); free(positions); stbi_image_free(img);
        return NULL;
    }

    decrypted[msg_len] = '\0';

    free(data); free(positions); stbi_image_free(img);
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




void calculate_and_save_mse_psnr_modified_only(const unsigned char *img1, const unsigned char *img2,
                                               int width, int height, int channels) {
    double mse[3] = {0.0, 0.0, 0.0};
    int count[3] = {0, 0, 0};
    int num_pixels = width * height;

    for (int i = 0; i < num_pixels; i++) {
        for (int c = 0; c < channels; c++) {
            int idx = i * channels + c;

            // Solo considerar píxeles donde hay diferencia
            if (img1[idx] != img2[idx]) {
                double diff = (double)img1[idx] - (double)img2[idx];
                mse[c] += diff * diff;
                count[c]++;
            }
        }
    }

    double mse_total = 0.0;
    double psnr[3];
    double psnr_total;
    const double MAX_PIXEL = 255.0;
    int modified_channels = 0;

    for (int c = 0; c < channels; c++) {
        if (count[c] > 0) {
            mse[c] /= count[c];
            psnr[c] = 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse[c]);
            mse_total += mse[c];
            modified_channels++;
        } else {
            mse[c] = 0.0;
            psnr[c] = INFINITY;
        }
    }

    if (modified_channels > 0) {
        mse_total /= modified_channels;
        psnr_total = 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_total);
    } else {
        mse_total = 0.0;
        psnr_total = INFINITY;
    }

    // Guardar resultados
    FILE *f = fopen("result_modified.txt", "w");
    if (!f) {
        perror("Error opening result_modified.txt");
        return;
    }

    fprintf(f, "MSE (R): %.6f (en %d píxeles modificados)\n", mse[0], count[0]);
    fprintf(f, "MSE (G): %.6f (en %d píxeles modificados)\n", mse[1], count[1]);
    fprintf(f, "MSE (B): %.6f (en %d píxeles modificados)\n", mse[2], count[2]);
    fprintf(f, "MSE (Average): %.6f\n\n", mse_total);

    fprintf(f, "PSNR (R): %.6f dB\n", psnr[0]);
    fprintf(f, "PSNR (G): %.6f dB\n", psnr[1]);
    fprintf(f, "PSNR (B): %.6f dB\n", psnr[2]);
    fprintf(f, "PSNR (Average): %.6f dB\n", psnr_total);

    fclose(f);
}
