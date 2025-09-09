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

/**
 * @file lsb2_image.c
 * @brief Esteganografía LSB2 para imágenes de 8 bits/canal (RGB/RGBA) con cifrado simétrico (libsodium).
 *
 * @details
 *  Este archivo implementa dos funciones principales:
 *   - embedMessage(): cifra y empaqueta un mensaje, lo inserta en 2 LSB por canal usando una permutación
 *     pseudo-aleatoria de posiciones derivada de la clave.
 *   - extractMessage(): recupera y descifra el mensaje previamente insertado.
 *
 *  **Formato del payload esteganográfico** (en bytes):
 *    [ "###" | NONCE(24) | MSG_LEN(4) | CIPHER_LEN(4) | CIPHER | "###" ]
 *
 */

/**
 * @brief Inserta (LSB2) un mensaje cifrado en la imagen indicada y guarda la salida.
 *
 * @param imagePath   Ruta de la imagen de entrada (8 bits/canal). Debe tener >= 3 canales.
 * @param message     Cadena a ocultar (UTF-8 sin terminador incluido en el cifrado).
 * @param outputPath  Nombre del fichero de salida (se ubicará en ./out/ vía build_path_static()).
 * @param key         Clave/contraseña (se deriva con crypto_generichash a 32 bytes para secretbox).
 *
 */
void embedMessage(const char *imagePath, const char *message, const char *outputPath, const char *key) {
    if (sodium_init() < 0) return; 

    // Carga de imagen (stb): width, height, channels (8-bit por canal)
    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) return; 

    // Conservamos copia del original para métricas (MSE/PSNR por imagen)
    unsigned char *img_original = (unsigned char *)malloc((size_t)width * height * channels);
    if (!img_original) { stbi_image_free(img); return; }
    memcpy(img_original, img, (size_t)width * height * channels);

    // --- Cifrado y empaquetado ---
    size_t messageLen = strlen(message);
    size_t cipherLen  = messageLen + crypto_secretbox_MACBYTES; 

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof nonce);

    unsigned char key_bin[crypto_secretbox_KEYBYTES];

    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    unsigned char *cipher = (unsigned char *)malloc(cipherLen);
    if (!cipher) { stbi_image_free(img); free(img_original); return; }
    crypto_secretbox_easy(cipher, (const unsigned char *)message, messageLen, nonce, key_bin);


    uint32_t msg_len_le    = (uint32_t)messageLen;
    uint32_t cipher_len_le = (uint32_t)cipherLen;

    // Marco: "###" | NONCE | msg_len(4) | cipher_len(4) | cipher | "###"
    size_t totalLen = 3 + crypto_secretbox_NONCEBYTES + 4 + 4 + cipherLen + 3;
    unsigned char *finalData = (unsigned char *)calloc(totalLen, 1);
    if (!finalData) { free(cipher); stbi_image_free(img); free(img_original); return; }

    unsigned char *ptr = finalData;
    memcpy(ptr, "###", 3);                                   ptr += 3;
    memcpy(ptr, nonce, crypto_secretbox_NONCEBYTES);          ptr += crypto_secretbox_NONCEBYTES;
    memcpy(ptr, &msg_len_le, 4);                              ptr += 4;
    memcpy(ptr, &cipher_len_le, 4);                           ptr += 4;
    memcpy(ptr, cipher, cipherLen);                           ptr += cipherLen;
    memcpy(ptr, "###", 3);

    // --- Cálculo de capacidad para LSB2 ---
    size_t bitsNeeded     = totalLen * 8;          // bits totales a insertar
    size_t pairsNeeded    = (bitsNeeded + 1) / 2;  // ceil(bits/2) pares de 2 bits
    size_t totalPositions = (size_t)width * height * 3; // Sólo RGB (3 componentes por pixel)

    if (pairsNeeded > totalPositions) {
        fprintf(stderr, "Message too large for LSB2 capacity.\n");
        free(cipher); free(finalData); stbi_image_free(img); free(img_original);
        return;
    }

    // --- Permutación Fisher–Yates con semilla derivada de la clave ---
    size_t *positions = (size_t *)malloc(totalPositions * sizeof(size_t));
    if (!positions) { free(cipher); free(finalData); stbi_image_free(img); free(img_original); return; }
    for (size_t i = 0; i < totalPositions; i++) positions[i] = i;

    unsigned char seed_hash[crypto_generichash_BYTES];
    crypto_generichash(seed_hash, sizeof seed_hash, (const unsigned char *)key, strlen(key), NULL, 0);
    uint32_t seed = ((uint32_t)seed_hash[0])
                  | ((uint32_t)seed_hash[1] << 8)
                  | ((uint32_t)seed_hash[2] << 16)
                  | ((uint32_t)seed_hash[3] << 24);
    srand(seed); 

    for (size_t i = totalPositions - 1; i > 0; i--) {
        size_t j = (size_t)(rand() % (i + 1));
        size_t tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }

    // --- Escritura LSB2 (dos bits por componente, orden MSB-first dentro de cada byte del payload) ---
    for (size_t pairIndex = 0; pairIndex < pairsNeeded; ++pairIndex) {
        size_t bitIndex = pairIndex * 2; 

        // b1 = bit más significativo del par; b2 = siguiente bit
        size_t bytePos1 = bitIndex / 8;          
        int bit1 = 7 - (int)(bitIndex % 8);
        unsigned char b1 = (unsigned char)((finalData[bytePos1] >> bit1) & 1);

        size_t bytePos2 = (bitIndex + 1) / 8;    
        int bit2 = 7 - (int)((bitIndex + 1) % 8);
        unsigned char b2 = (unsigned char)((finalData[bytePos2] >> bit2) & 1);

        unsigned char pair = (unsigned char)((b1 << 1) | b2);

        size_t pos   = positions[pairIndex];
        size_t pixel = pos / 3;                  // índice de píxel (RGB)
        size_t color = pos % 3;                  // 0=R,1=G,2=B

        size_t idx = (size_t)pixel * channels + color; 
        img[idx] = (unsigned char)((img[idx] & (unsigned char)~3) | pair); // ~3 = 11111100, inserta 2 LSB
    }

    // Guardar PNG de salida en ./out/
    stbi_write_png(build_path_static(outputPath), width, height, channels, img, width * channels);

    // Métricas por imagen (MSE/PSNR por canal y agregados)
    calculate_metrics_image(img_original, img, width, height, channels, "./out/image_metrics.txt");

    // Limpieza
    free(cipher); free(finalData); free(positions);
    stbi_image_free(img); free(img_original);
}

/**
 * @brief Extrae y descifra el mensaje oculto (LSB2) de una imagen.
 *
 * @param imagePath Ruta de la imagen estego.
 * @param key       Clave/contraseña (debe coincidir con la usada en embedMessage()).
 * @return Cadena NUL-terminada con el mensaje en claro (heap). El llamador debe `free()`. NULL si falla.
 *
 */
char *extractMessage(const char *imagePath, const char *key) {
    if (sodium_init() < 0) return NULL;

    int width, height, channels;
    unsigned char *img = stbi_load(imagePath, &width, &height, &channels, 0);
    if (!img || channels < 3) return NULL;

    size_t totalPositions = (size_t)width * height * 3; 
    size_t *positions = (size_t *)malloc(totalPositions * sizeof(size_t));
    if (!positions) { stbi_image_free(img); return NULL; }
    for (size_t i = 0; i < totalPositions; i++) positions[i] = i;

    // Permutación determinista (igual que en embed)
    unsigned char seed_hash[crypto_generichash_BYTES];
    crypto_generichash(seed_hash, sizeof seed_hash, (const unsigned char *)key, strlen(key), NULL, 0);
    uint32_t seed = ((uint32_t)seed_hash[0])
                  | ((uint32_t)seed_hash[1] << 8)
                  | ((uint32_t)seed_hash[2] << 16)
                  | ((uint32_t)seed_hash[3] << 24);
    srand(seed);

    for (size_t i = totalPositions - 1; i > 0; i--) {
        size_t j = (size_t)(rand() % (i + 1));
        size_t tmp = positions[i]; positions[i] = positions[j]; positions[j] = tmp;
    }

    // --- Extraer primero la cabecera (para saber cipher_len) ---
    size_t headerLenBytes = 3 + crypto_secretbox_NONCEBYTES + 4 + 4; // "###" + nonce + msg_len + cipher_len
    size_t headerBits     = headerLenBytes * 8;
    size_t headerPairs    = (headerBits + 1) / 2;

    unsigned char headerBuf[64] = {0}; 

    for (size_t pairIndex = 0; pairIndex < headerPairs; ++pairIndex) {
        size_t pos   = positions[pairIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        size_t idx   = (size_t)pixel * channels + color;

        unsigned char pair = (unsigned char)(img[idx] & 3);

        // Escribimos los 2 bits del par en el buffer en orden MSB-first
        for (int k = 0; k < 2; ++k) {
            size_t bitIndex = pairIndex * 2 + (size_t)k;
            if (bitIndex >= headerBits) break; // salvaguarda
            size_t bytePos = bitIndex / 8;    int bitInByte = 7 - (int)(bitIndex % 8);
            unsigned char bitVal = (unsigned char)((pair >> (1 - k)) & 1);
            headerBuf[bytePos] |= (unsigned char)(bitVal << bitInByte);
        }
    }

    // Comprobación de marcador inicial
    if (memcmp(headerBuf, "###", 3) != 0) {
        fprintf(stderr, "No initial marker found.\n");
        free(positions); stbi_image_free(img);
        return NULL;
    }

    // Parseo de cabecera
    unsigned char *ptr = headerBuf + 3;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, ptr, crypto_secretbox_NONCEBYTES); ptr += crypto_secretbox_NONCEBYTES;

    uint32_t msg_len, cipher_len;
    memcpy(&msg_len, ptr, 4); ptr += 4;
    memcpy(&cipher_len, ptr, 4); ptr += 4;

    // --- Extraer el bloque completo (cabecera + cipher + marcador final) ---
    size_t totalLenBytes = 3 + crypto_secretbox_NONCEBYTES + 4 + 4 + (size_t)cipher_len + 3;
    size_t bitsToExtract = totalLenBytes * 8;
    size_t totalPairs    = (bitsToExtract + 1) / 2;

    unsigned char *data = (unsigned char *)calloc(totalLenBytes, 1);
    if (!data) { free(positions); stbi_image_free(img); return NULL; }

    for (size_t pairIndex = 0; pairIndex < totalPairs; ++pairIndex) {
        size_t pos   = positions[pairIndex];
        size_t pixel = pos / 3;
        size_t color = pos % 3;
        size_t idx   = (size_t)pixel * channels + color;

        unsigned char pair = (unsigned char)(img[idx] & 3);

        for (int k = 0; k < 2; ++k) {
            size_t bitIndex = pairIndex * 2 + (size_t)k;
            if (bitIndex >= bitsToExtract) break;
            size_t bytePos = bitIndex / 8;    int bitInByte = 7 - (int)(bitIndex % 8);
            unsigned char bitVal = (unsigned char)((pair >> (1 - k)) & 1);
            data[bytePos] |= (unsigned char)(bitVal << bitInByte);
        }
    }

    // Verificación de marcador final
    if (memcmp(data + totalLenBytes - 3, "###", 3) != 0) {
        fprintf(stderr, "No final marker found.\n");
        free(positions); stbi_image_free(img); free(data);
        return NULL;
    }

    // Puntero al ciphertext dentro del buffer total
    unsigned char *cipher = data + 3 + crypto_secretbox_NONCEBYTES + 4 + 4;

    // Derivar la clave binaria de la contraseña
    unsigned char key_bin[crypto_secretbox_KEYBYTES];
    crypto_generichash(key_bin, sizeof key_bin, (const unsigned char *)key, strlen(key), NULL, 0);

    // Descifrar (se reserva +1 byte para NUL terminador)
    unsigned char *decrypted = (unsigned char *)malloc((size_t)msg_len + 1);
    if (!decrypted) { free(data); free(positions); stbi_image_free(img); return NULL; }

    if (crypto_secretbox_open_easy(decrypted, cipher, cipher_len, nonce, key_bin) != 0) {
        fprintf(stderr, "Decryption failed.\n");
        free(decrypted); free(data); free(positions); stbi_image_free(img);
        return NULL; 
    }

    decrypted[msg_len] = '\0'; 


    free(data); free(positions); stbi_image_free(img);
    return (char *)decrypted; 
}

// ============================
// Métricas de imagen (MSE/PSNR)
// ============================

/**
 * @brief Nombre corto del canal para informes (R,G,B o C# genérico).
 */
static const char *chan_name(int c, int channels) {
    if (channels == 3) {
        static const char *names[3] = {"R","G","B"};
        return names[c];
    }
    static char buf[12];
    snprintf(buf, sizeof buf, "C%d", c);
    return buf;
}

/**
 * @brief Calcula MSE/PSNR por canal y agregados entre dos imágenes (8 bits/canal) del mismo tamaño.
 *
 * @param img1,img2  Buffers de imagen (intercalados por canal: RGB[A]RGB[A]...).
 * @param width,height,channels Dimensiones y nº de canales.
 * @param output_file Ruta de salida para el informe (texto). Si es NULL, escribe "image_metrics.txt".
 *
 * @note Considera un píxel "modificado" si cambia **cualquier** canal. PSNR usa pico=255.
 */
void calculate_metrics_image(const unsigned char *img1, const unsigned char *img2,
                                       int width, int height, int channels,
                                       const char *output_file)
{
    const int num_pixels = width * height;
    const double MAX_PIXEL = 255.0;

    // Acumuladores por canal
    double *sumsq_all = (double *)calloc((size_t)channels, sizeof(double));
    double *sumsq_mod = (double *)calloc((size_t)channels, sizeof(double));
    int    *count_mod = (int *)calloc((size_t)channels, sizeof(int));
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

    // Buffers para MSE/PSNR por canal
    double *mse_all  = (double *)calloc((size_t)channels, sizeof(double));
    double *psnr_all = (double *)calloc((size_t)channels, sizeof(double));
    double *mse_mod  = (double *)calloc((size_t)channels, sizeof(double));
    double *psnr_mod = (double *)calloc((size_t)channels, sizeof(double));
    double *mse_unm  = (double *)calloc((size_t)channels, sizeof(double));
    double *psnr_unm = (double *)calloc((size_t)channels, sizeof(double));
    if (!mse_all || !psnr_all || !mse_mod || !psnr_mod || !mse_unm || !psnr_unm) {
        perror("calloc");
        free(sumsq_all); free(sumsq_mod); free(count_mod);
        free(mse_all); free(psnr_all); free(mse_mod); free(psnr_mod); free(mse_unm); free(psnr_unm);
        return;
    }

    double mse_all_avg = 0.0, mse_mod_avg = 0.0, mse_unm_avg = 0.0;
    int channels_with_mod = 0, channels_with_unm = 0;

    for (int c = 0; c < channels; ++c) {
        // Todos los píxeles
        mse_all[c] = sumsq_all[c] / (double)num_pixels;
        psnr_all[c] = (mse_all[c] == 0.0) ? INFINITY
                                          : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_all[c]);
        mse_all_avg += mse_all[c];

        // Solo modificados
        if (count_mod[c] > 0) {
            mse_mod[c]  = sumsq_mod[c] / (double)count_mod[c];
            psnr_mod[c] = (mse_mod[c] == 0.0) ? INFINITY
                                              : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_mod[c]);
            mse_mod_avg += mse_mod[c];
            channels_with_mod++;
        } else {
            mse_mod[c]  = 0.0;
            psnr_mod[c] = INFINITY;
        }

        // No modificados
        int count_unm = num_pixels - count_mod[c];
        if (count_unm > 0) {
            mse_unm[c]  = 0.0;
            psnr_unm[c] = INFINITY;
            channels_with_unm++;
        } else {
            mse_unm[c]  = 0.0;
            psnr_unm[c] = INFINITY;
        }
    }

    mse_all_avg /= (double)channels;
    if (channels_with_mod > 0) mse_mod_avg /= (double)channels_with_mod;
    if (channels_with_unm > 0) mse_unm_avg /= (double)channels_with_unm;

    double psnr_all_avg = (mse_all_avg == 0.0) ? INFINITY
                                               : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_all_avg);
    double psnr_mod_avg = (channels_with_mod == 0 || mse_mod_avg == 0.0) ? INFINITY
                                                                         : 10.0 * log10((MAX_PIXEL * MAX_PIXEL) / mse_mod_avg);
    double psnr_unm_avg = INFINITY; // si hay no modificados, su MSE es 0 → PSNR infinito

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
    for (int c = 0; c < channels; ++c) fprintf(f, "MSE (%s): %.6f\n", chan_name(c, channels), mse_all[c]);
    fprintf(f, "MSE (Average): %.6f\n", mse_all_avg);
    for (int c = 0; c < channels; ++c) fprintf(f, "PSNR (%s): %.6f dB\n", chan_name(c, channels), psnr_all[c]);
    fprintf(f, "PSNR (Average): %.6f dB\n\n", psnr_all_avg);

    // --- Sección 2: SOLO MODIFICADOS ---
    fprintf(f, "==== SOLO MODIFICADOS ====\n");
    for (int c = 0; c < channels; ++c) fprintf(f, "Canal %s: muestras modificadas = %d\n", chan_name(c, channels), count_mod[c]);
    for (int c = 0; c < channels; ++c) fprintf(f, "MSE (%s): %.6f\n", chan_name(c, channels), mse_mod[c]);
    fprintf(f, "MSE (Average sobre canales con cambios): %.6f\n", mse_mod_avg);
    for (int c = 0; c < channels; ++c) {
        fprintf(f, "PSNR (%s): %s%.6f dB\n",
                chan_name(c, channels), (count_mod[c]==0 ? "(sin datos) " : ""), psnr_mod[c]);
    }
    fprintf(f, "PSNR (Average): %s%.6f dB\n\n", (channels_with_mod==0 ? "(sin datos) " : ""), psnr_mod_avg);

    // --- Sección 3: NO MODIFICADOS ---
    fprintf(f, "==== NO MODIFICADOS ====\n");
    for (int c = 0; c < channels; ++c) {
        int count_unm = num_pixels - count_mod[c];
        fprintf(f, "Canal %s: muestras no modificadas = %d\n", chan_name(c, channels), count_unm);
    }
    for (int c = 0; c < channels; ++c) fprintf(f, "MSE (%s): %.6f\n", chan_name(c, channels), mse_unm[c]);
    fprintf(f, "MSE (Average sobre canales con no modificados): %.6f\n", mse_unm_avg);
    for (int c = 0; c < channels; ++c) fprintf(f, "PSNR (%s): %.6f dB\n", chan_name(c, channels), psnr_unm[c]);
    fprintf(f, "PSNR (Average): %.6f dB\n", psnr_unm_avg);

    fclose(f);

    free(sumsq_all); free(sumsq_mod); free(count_mod);
    free(mse_all); free(psnr_all); free(mse_mod); free(psnr_mod); free(mse_unm); free(psnr_unm);
}
