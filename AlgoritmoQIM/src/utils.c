#include "../headers/utils.h"
#include <stdio.h>
#include <sndfile.h>
#include <math.h>
#include <stdlib.h>

void print_usage() {
    printf("Usage:\n");
    printf("  ./algoritmoqim embed <in.wav> <out.wav> <message>\n");
    printf("  ./algoritmoqim embedfile <in.wav> <out.wav> <message.txt>\n");
    printf("  ./algoritmoqim extract <in.wav> <msg_len>\n");
    printf("  ./algoritmoqim metrics <original.wav> <stego.wav> <output.txt>\n");
}

void calculate_mse_psnr(const char *original_file, const char *stego_file, const char *output_txt, sf_count_t samples_to_check) {
    SF_INFO sfinfo1, sfinfo2;
    SNDFILE *f1 = sf_open(original_file, SFM_READ, &sfinfo1);
    SNDFILE *f2 = sf_open(stego_file, SFM_READ, &sfinfo2);

    if (!f1 || !f2) {
        printf("Error opening files\n");
        if (f1) sf_close(f1);
        if (f2) sf_close(f2);
        return;
    }

    if (sfinfo1.frames != sfinfo2.frames || sfinfo1.channels != sfinfo2.channels) {
        printf("Files must have the same length and channels\n");
        sf_close(f1); sf_close(f2);
        return;
    }

    float buf1[1024], buf2[1024];
    sf_count_t total = 0;
    double mse = 0.0;
    sf_count_t read1, read2;
    sf_count_t samples_compared = 0;
    int channels = sfinfo1.channels;

    while ((read1 = sf_readf_float(f1, buf1, 1024)) > 0 &&
           (read2 = sf_readf_float(f2, buf2, 1024)) > 0 &&
           samples_compared < samples_to_check) {
        
        sf_count_t frames_to_process = read1;
        // No procesar más muestras que el límite
        if ((samples_compared + read1 * channels) > samples_to_check) {
            frames_to_process = (samples_to_check - samples_compared) / channels;
        }

        for (int i = 0; i < frames_to_process * channels; ++i) {
            double diff = buf1[i] - buf2[i];
            mse += diff * diff;
            total++;
        }

        samples_compared += frames_to_process * channels;

        if (samples_compared >= samples_to_check) {
            break;
        }
    }

    mse /= total;
    double psnr = 10.0 * log10(1.0 / mse);

    FILE *out = fopen(output_txt, "w");
    if (!out) {
        printf("Error opening output file\n");
    } else {
        fprintf(out, "MSE (modified part): %.8f\n", mse);
        fprintf(out, "PSNR (modified part): %.2f dB\n", psnr);
        fclose(out);
        printf("Results saved to %s\n", output_txt);
    }

    sf_close(f1);
    sf_close(f2);
}

char *read_text_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    printf("Intentando abrir: %s\n", filename);
    if (!f) {
        printf("Error opening file: %s\n", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buffer = (char*)malloc(len + 1);
    if (!buffer) {
        fclose(f);
        printf("Memory allocation failed\n");
        return NULL;
    }
    fread(buffer, 1, len, f);
    buffer[len] = '\0';
    fclose(f);
    return buffer;
}