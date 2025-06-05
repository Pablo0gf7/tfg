#ifndef UTILS_H
#define UTILS_H
#include <sndfile.h>
void print_usage();
void calculate_mse_psnr(const char *original_file, const char *stego_file, const char *output_txt, sf_count_t samples_to_check);
char *read_text_file(const char *filename);
#endif // UTILS_H