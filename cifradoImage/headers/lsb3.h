#ifndef LSB3_H
#define LSB3_H

#include <stddef.h>

void embedMessage(const char *imagePath, const char *message, const char *outputPath, const char *key);
char *extractMessage(const char *imagePath, const char *key);
void calculate_and_save_mse_psnr(const unsigned char *img1, const unsigned char *img2,
                                int width, int height, int channels,
                                const char *output_file);

const char *build_path_static( const char *filename);

#endif // LSB3_H