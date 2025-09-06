#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb_image_write.h"

#include "../headers/lsb.h"
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

    // --- Cifrado y empaquetado (igual que antes) ---
    size_t messageLen = strlen(message);
    size_t cipherLen  = messageLen + crypto_secretbox_MACBYTES;

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof nonce);

    unsigned char key_bin[crypto_secretbox_KEYBYTES];
    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    unsigned char *cipher = malloc(cipherLen);
    crypto_secretbox_easy(cipher, (const unsigned char *)message, messageLen, nonce, key_bin);

    uint32_t msg_len_le    = (uint32_t)messageLen;
    uint32_t cipher_len_le = (uint32_t)cipherLen;

    size_t totalLen = 3 + crypto_secretbox_NONCEBYTES + 4 + 4 + cipherLen + 3;
    unsigned char *finalData = calloc(totalLen, 1);
    unsigned char *ptr = finalData;

    memcpy(ptr, "###", 3);                                   ptr += 3;
    memcpy(ptr, nonce, crypto_secretbox_NONCEBYTES);          ptr += crypto_secretbox_NONCEBYTES;
    memcpy(ptr, &msg_len_le, 4);                              ptr += 4;
    memcpy(ptr, &cipher_len_le, 4);                           ptr += 4;
    memcpy(ptr, cipher, cipherLen);                           ptr += cipherLen;
    memcpy(ptr, "###", 3);

    // --- LSB2: capacidad en PARES de bits (2 bits por posición) ---
    size_t bitsNeeded    = totalLen * 8;
    size_t pairsNeeded   = (bitsNeeded + 1) / 2;              // ceil(bits/2)
    size_t totalPositions = (size_t)width * height * 3;       // R,G,B

    if (pairsNeeded > totalPositions) {
        fprintf(stderr, "Message too large for LSB2 capacity.\n");
        free(cipher); free(finalData); stbi_image_free(img); free(img_original);
        return;
    }

    // --- Permutación Fisher–Yates con semilla derivada ---
    size_t *positions = malloc(totalPositions * sizeof(size_t));
    for (size_t i = 0; i < totalPositions; i++) positions[i] = i;

    unsigned char seed_hash[crypto_generichash_BYTES];
    crypto_generichash(seed_hash, sizeof seed_hash, (const unsigned char *)key, strlen(key), NULL, 0);
    uint32_t seed = ((uint32_t)seed_hash[0])
                  | ((uint32_t)seed_hash[1] << 8)
                  | ((uint32_t)seed_hash[2] << 16)
                  | ((uint32_t)seed_hash[3] << 24);
    srand(seed);

    for (size_t i = totalPositions - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        size_t tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }

    // --- Escritura LSB2 (MSB-first por byte) ---
    for (size_t pairIndex = 0; pairIndex < pairsNeeded; ++pairIndex) {
        size_t bitIndex = pairIndex * 2; // primer bit del par dentro del stream

        // b1 = bit MSB del par, b2 = siguiente bit (MSB->LSB)
        size_t bytePos1 = bitIndex / 8;
        int    bit1     = 7 - (bitIndex % 8);
        unsigned char b1 = (finalData[bytePos1] >> bit1) & 1;

        size_t bytePos2 = (bitIndex + 1) / 8;
        int    bit2     = 7 - ((bitIndex + 1) % 8);
        unsigned char b2 = (finalData[bytePos2] >> bit2) & 1;

        unsigned char pair = (unsigned char)((b1 << 1) | b2);

        size_t pos   = positions[pairIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;

        size_t idx = (size_t)pixel * channels + color;
        img[idx] = (img[idx] & (unsigned char)~3) | pair; // ~3 = 11111100
    }

    stbi_write_png(build_path_static(outputPath), width, height, channels, img, width * channels);

    calculate_metrics_image(img_original, img, width, height, channels, "./out/image_metrics.txt");

    free(cipher); free(finalData); free(positions);
    stbi_image_free(img); free(img_original);
}

char *extractMessage(const char *imagePath, const char *key) {
    if (sodium_init() < 0) return NULL;

    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) return NULL;

    size_t totalPositions = (size_t)width * height * 3;
    size_t *positions = malloc(totalPositions * sizeof(size_t));
    for (size_t i = 0; i < totalPositions; i++) positions[i] = i;

    unsigned char seed_hash[crypto_generichash_BYTES];
    crypto_generichash(seed_hash, sizeof seed_hash, (const unsigned char *)key, strlen(key), NULL, 0);
    uint32_t seed = ((uint32_t)seed_hash[0])
                  | ((uint32_t)seed_hash[1] << 8)
                  | ((uint32_t)seed_hash[2] << 16)
                  | ((uint32_t)seed_hash[3] << 24);
    srand(seed);

    for (size_t i = totalPositions - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        size_t tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }

    // --- Primero extraemos SOLO la cabecera para conocer cipher_len ---
    size_t headerLenBytes = 3 + crypto_secretbox_NONCEBYTES + 4 + 4; // "###" + nonce + msg_len + cipher_len
    size_t headerBits     = headerLenBytes * 8;
    size_t headerPairs    = (headerBits + 1) / 2;

    unsigned char headerBuf[64] = {0}; // suficiente para la cabecera

    for (size_t pairIndex = 0; pairIndex < headerPairs; ++pairIndex) {
        size_t pos   = positions[pairIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        size_t idx   = (size_t)pixel * channels + color;

        unsigned char pair = img[idx] & 3;

        // Escribimos los dos bits del par en el buffer (MSB-first), cuidando del final impar
        for (int k = 0; k < 2; ++k) {
            size_t bitIndex = pairIndex * 2 + (size_t)k;
            if (bitIndex >= headerBits) break; // podría pasar si headerBits es impar (no lo es aquí, pero por robustez)

            size_t bytePos = bitIndex / 8;
            int bitInByte  = 7 - (bitIndex % 8);
            unsigned char bitVal = (unsigned char)((pair >> (1 - k)) & 1);
            headerBuf[bytePos] |= (unsigned char)(bitVal << bitInByte);
        }
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

    // --- Ahora extraemos TODO: cabecera + cipher + "###" final ---
    size_t totalLenBytes = 3 + crypto_secretbox_NONCEBYTES + 4 + 4 + (size_t)cipher_len + 3;
    size_t bitsToExtract = totalLenBytes * 8;
    size_t totalPairs    = (bitsToExtract + 1) / 2;

    unsigned char *data = calloc(totalLenBytes, 1);

    for (size_t pairIndex = 0; pairIndex < totalPairs; ++pairIndex) {
        size_t pos   = positions[pairIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        size_t idx   = (size_t)pixel * channels + color;

        unsigned char pair = img[idx] & 3;

        for (int k = 0; k < 2; ++k) {
            size_t bitIndex = pairIndex * 2 + (size_t)k;
            if (bitIndex >= bitsToExtract) break;

            size_t bytePos = bitIndex / 8;
            int bitInByte  = 7 - (bitIndex % 8);
            unsigned char bitVal = (unsigned char)((pair >> (1 - k)) & 1);
            data[bytePos] |= (unsigned char)(bitVal << bitInByte);
        }
    }

    if (memcmp(data + totalLenBytes - 3, "###", 3) != 0) {
        fprintf(stderr, "No final marker found.\n");
        free(positions); stbi_image_free(img); free(data);
        return NULL;
    }

    unsigned char *cipher = data + 3 + crypto_secretbox_NONCEBYTES + 4 + 4;

    unsigned char key_bin[crypto_secretbox_KEYBYTES];
    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    unsigned char *decrypted = malloc((size_t)msg_len + 1);
    if (crypto_secretbox_open_easy(decrypted, cipher, cipher_len, nonce, key_bin) != 0) {
        fprintf(stderr, "Decryption failed.\n");
        free(decrypted); free(data); free(positions); stbi_image_free(img);
        return NULL;
    }

    decrypted[msg_len] = '\0';

    free(data); free(positions); stbi_image_free(img);
    return (char *)decrypted;
}




static const char *chan_name(int c, int channels) {
    // Etiquetas bonitas para RGB; genéricas para otros casos
    if (channels == 3) {
        static const char *names[3] = {"R","G","B"};
        return names[c];
    }
    static char buf[10];
    snprintf(buf, sizeof buf, "C%d", c);
    return buf;
}

void calculate_metrics_image(const unsigned char *img1, const unsigned char *img2,
                                       int width, int height, int channels,
                                       const char *output_file)
{
    const int num_pixels = width * height;
    const double MAX_PIXEL = 255.0;

    // Acumuladores por canal
    double *sumsq_all = calloc(channels, sizeof(double));
    double *sumsq_mod = calloc(channels, sizeof(double));
    int    *count_mod = calloc(channels, sizeof(int));
    if (!sumsq_all || !sumsq_mod || !count_mod) {
        perror("calloc");
        free(sumsq_all); free(sumsq_mod); free(count_mod);
        return;
    }

    // Contadores de píxeles a nivel global (modificado si cambia algún canal)
    int modified_pixels_any = 0;
    int unmodified_pixels_all = 0;

    for (int i = 0; i < num_pixels; ++i) {
        int pixel_modified = 0;
        for (int c = 0; c < channels; ++c) {
            int idx = i * channels + c;
            double diff = (double)img1[idx] - (double)img2[idx];
            double d2   = diff * diff;

            sumsq_all[c] += d2;

            if (diff != 0.0) {
                sumsq_mod[c] += d2;
                count_mod[c] += 1;
                pixel_modified = 1;
            }
        }
        if (pixel_modified) modified_pixels_any++;
        else unmodified_pixels_all++;
    }

    // MSE/PSNR "todos"
    double *mse_all  = calloc(channels, sizeof(double));
    double *psnr_all = calloc(channels, sizeof(double));
    // MSE/PSNR "solo modificados"
    double *mse_mod  = calloc(channels, sizeof(double));
    double *psnr_mod = calloc(channels, sizeof(double));
    // MSE/PSNR "no modificados" (serán 0 e INF si realmente no cambiaron)
    double *mse_unm  = calloc(channels, sizeof(double));
    double *psnr_unm = calloc(channels, sizeof(double));
    if (!mse_all || !psnr_all || !mse_mod || !psnr_mod || !mse_unm || !psnr_unm) {
        perror("calloc");
        free(sumsq_all); free(sumsq_mod); free(count_mod);
        free(mse_all); free(psnr_all); free(mse_mod); free(psnr_mod); free(mse_unm); free(psnr_unm);
        return;
    }

    double mse_all_avg = 0.0;
    double mse_mod_avg = 0.0;
    double mse_unm_avg = 0.0;

    int channels_with_mod = 0;
    int channels_with_unm = 0;

    for (int c = 0; c < channels; ++c) {
        // Todos los píxeles
        mse_all[c] = sumsq_all[c] / (double)num_pixels;
        psnr_all[c] = (mse_all[c] == 0.0) ? INFINITY
                                          : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_all[c]);
        mse_all_avg += mse_all[c];

        // Solo modificados (si hubo alguno en ese canal)
        if (count_mod[c] > 0) {
            mse_mod[c] = sumsq_mod[c] / (double)count_mod[c];
            psnr_mod[c] = (mse_mod[c] == 0.0) ? INFINITY
                                              : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_mod[c]);
            mse_mod_avg += mse_mod[c];
            channels_with_mod++;
        } else {
            mse_mod[c]  = 0.0;
            psnr_mod[c] = INFINITY; // sin muestras modificadas en este canal
        }

        // No modificados: por definición diff=0 en esas muestras -> MSE=0, PSNR=INF.
        // Aun así, reportamos cuántas hay por canal.
        int count_unm = num_pixels - count_mod[c];
        if (count_unm > 0) {
            mse_unm[c]  = 0.0;
            psnr_unm[c] = INFINITY;
            channels_with_unm++;
        } else {
            mse_unm[c]  = 0.0;
            psnr_unm[c] = INFINITY; // no hay muestras no modificadas en este canal
        }
    }

    mse_all_avg /= (double)channels;

    if (channels_with_mod > 0)  mse_mod_avg /= (double)channels_with_mod;
    if (channels_with_unm > 0)  mse_unm_avg /= (double)channels_with_unm;

    double psnr_all_avg = (mse_all_avg == 0.0) ? INFINITY
                                               : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_all_avg);
    double psnr_mod_avg = (channels_with_mod == 0 || mse_mod_avg == 0.0) ? INFINITY
                                                                         : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_mod_avg);
    double psnr_unm_avg = INFINITY; // siempre 0 → ∞ mientras haya alguna muestra no modificada

    FILE *f = fopen(output_file ? output_file : "image_metrics.txt", "w");
    if (!f) {
        perror("Error opening metrics file");
        free(sumsq_all); free(sumsq_mod); free(count_mod);
        free(mse_all); free(psnr_all); free(mse_mod); free(psnr_mod); free(mse_unm); free(psnr_unm);
        return;
    }

    fprintf(f, "Imagen: %dx%d, canales=%d, pixeles=%d\n", width, height, channels, num_pixels);
    fprintf(f, "Pixeles modificados (al menos un canal): %d (%.4f %%)\n",
            modified_pixels_any, 100.0 * (double)modified_pixels_any / (double)num_pixels);
    fprintf(f, "Pixeles NO modificados: %d (%.4f %%)\n\n",
            unmodified_pixels_all, 100.0 * (double)unmodified_pixels_all / (double)num_pixels);

    // --- Sección 1: TODOS los píxeles ---
    fprintf(f, "==== TODOS LOS PIXELES ====\n");
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "MSE (%s): %.6f\n", chan_name(c, channels), mse_all[c]);
    }
    fprintf(f, "MSE (Average): %.6f\n", mse_all_avg);
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "PSNR (%s): %.6f dB\n", chan_name(c, channels), psnr_all[c]);
    }
    fprintf(f, "PSNR (Average): %.6f dB\n\n", psnr_all_avg);

    // --- Sección 2: SOLO MODIFICADOS ---
    fprintf(f, "==== SOLO MODIFICADOS ====\n");
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "Canal %s: muestras modificadas = %d\n", chan_name(c, channels), count_mod[c]);
    }
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "MSE (%s): %.6f\n", chan_name(c, channels), mse_mod[c]);
    }
    fprintf(f, "MSE (Average sobre canales con cambios): %.6f\n", mse_mod_avg);
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "PSNR (%s): %s%.6f dB\n",
                chan_name(c, channels),
                (count_mod[c]==0 ? "(sin datos) " : ""),
                psnr_mod[c]);
    }
    fprintf(f, "PSNR (Average): %s%.6f dB\n\n",
            (channels_with_mod==0 ? "(sin datos) " : ""), psnr_mod_avg);

    // --- Sección 3: NO MODIFICADOS ---
    fprintf(f, "==== NO MODIFICADOS ====\n");
    for (int c = 0; c < channels; ++c) {
        int count_unm = num_pixels - count_mod[c];
        fprintf(f, "Canal %s: muestras no modificadas = %d\n", chan_name(c, channels), count_unm);
    }
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "MSE (%s): %.6f\n", chan_name(c, channels), mse_unm[c]); // será 0.0
    }
    fprintf(f, "MSE (Average sobre canales con no modificados): %.6f\n", mse_unm_avg); // será 0.0 si existe alguna
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "PSNR (%s): %.6f dB\n", chan_name(c, channels), psnr_unm[c]); 
    }
    fprintf(f, "PSNR (Average): %.6f dB\n", psnr_unm_avg); 

    fclose(f);

    free(sumsq_all); free(sumsq_mod); free(count_mod);
    free(mse_all); free(psnr_all); free(mse_mod); free(psnr_mod); free(mse_unm); free(psnr_unm);
}