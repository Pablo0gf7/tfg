#include "../headers/utils.h"
#include <stdio.h>
#include <sndfile.h>
#include <math.h>

void print_usage() {
    printf("Usage:\n");
    printf("  ./algoritmoqim embed <in.wav> <out.wav> <message>\n");
    printf("  ./algoritmoqim extract <in.wav> <msg_len>\n");
    printf("  ./algoritmoqim metrics <original.wav> <stego.wav> <output.txt>\n");
}

void calculate_mse_psnr(const char *original_file, const char *stego_file, const char *output_txt) {
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
    while ((read1 = sf_readf_float(f1, buf1, 1024)) > 0 &&
           (read2 = sf_readf_float(f2, buf2, 1024)) > 0) {
        for (int i = 0; i < read1 * sfinfo1.channels; ++i) {
            double diff = buf1[i] - buf2[i];
            mse += diff * diff;
            total++;
        }
    }
    mse /= total;
    double psnr = 10.0 * log10(1.0 / mse); // Asumiendo normalización [-1,1]

    FILE *out = fopen(output_txt, "w");
    if (!out) {
        printf("Error opening output file\n");
    } else {
        fprintf(out, "MSE: %.8f\n", mse);
        fprintf(out, "PSNR: %.2f dB\n", psnr);
        fclose(out);
        printf("Results saved to %s\n", output_txt);
    }

    sf_close(f1);
    sf_close(f2);
}