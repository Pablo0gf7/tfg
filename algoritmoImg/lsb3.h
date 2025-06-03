#ifndef LSB3_H
#define LSB3_H

#include <stddef.h>

void embedMessage(const char *imagePath, const char *message, const char *outputPath);
char *extractMessage(const char *imagePath);

#endif // LSB3_H