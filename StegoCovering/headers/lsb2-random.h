#ifndef LSB3_H
#define LSB3_H

#include <stddef.h>

/**
 * @brief Inserta un mensaje en una imagen utilizando el algoritmo LSB3.
 * 
 * @param imagePath Ruta de la imagen de entrada.
 * @param message Mensaje a ocultar.
 * @param outputPath Ruta de la imagen de salida con el mensaje oculto.
 * @param key Clave para cifrado o aleatorización (si aplica).
 */
void embedMessage(const char *imagePath, const char *message, const char *outputPath, const char *key);

/**
 * @brief Extrae un mensaje oculto de una imagen utilizando el algoritmo LSB3.
 * 
 * @param imagePath Ruta de la imagen de entrada.
 * @param key Clave utilizada para la extracción (si aplica).
 * @return char* Mensaje extraído (debe ser liberado por el usuario).
 */
char *extractMessage(const char *imagePath, const char *key);

/**
 * @brief Calcula y guarda el MSE y PSNR entre dos imágenes.
 * 
 * @param img1 Puntero a los datos de la primera imagen.
 * @param img2 Puntero a los datos de la segunda imagen.
 * @param width Ancho de las imágenes.
 * @param height Alto de las imágenes.
 * @param channels Número de canales de color.
 * @param output_file Ruta del archivo donde se guardarán los resultados.
 */
void calculate_and_save_mse_psnr(const unsigned char *img1, const unsigned char *img2,
                                 int width, int height, int channels,
                                 const char *output_file);

                                 void calculate_and_save_mse_psnr_modified_only(const unsigned char *img1, const unsigned char *img2,
                                               int width, int height, int channels);

#endif // LSB3_H