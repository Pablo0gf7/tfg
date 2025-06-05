#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

// Función para codificar en Base64
void base64_encode(const unsigned char* input, int len, char* output) {
    EVP_EncodeBlock((unsigned char*)output, input, len);
}

// Función para decodificar Base64
int base64_decode(const char* input, unsigned char* output) {
    int len = strlen(input);
    return EVP_DecodeBlock(output, (const unsigned char*)input, len);
}

int main() {
    // Texto original
    const char* original_text = "El oeste de Texas divide la frontera entre Mexico y Nuevo México. Es muy bella pero aspera, llena de cactus, en esta region se encuentran las Davis Mountains. Todo el terreno esta lleno de piedra caliza, torcidos arboles de mezquite y espinosos nopales. Para admirar la verdadera belleza desertica, visite el Parque Nacional de Big Bend, cerca de Brownsville. Es el lugar favorito para los excurcionistas, acampadores y entusiastas de las rocas. Pequeños pueblos y ranchos se encuentran a lo largo de las planicies y cañones de esta region. El area solo tiene dos estaciones, tibia y realmente caliente. La mejor epoca para visitarla es de Diciembre a Marzo cuando los dias son tibios, las noches son frescas y florecen las plantas del desierto con la humedad en el aire.";
    printf("Texto original: %s\n", original_text);

    // Codificación
    int input_len = strlen(original_text);
    int encoded_len = 4 * ((input_len + 2) / 3); // longitud base64
    char encoded[encoded_len + 1];
    memset(encoded, 0, sizeof(encoded));

    base64_encode((const unsigned char*)original_text, input_len, encoded);
    printf("Codificado en Base64: %s\n", encoded);

    // Decodificación
    unsigned char decoded[input_len + 1]; // Puede ser un poco más grande si quieres
    memset(decoded, 0, sizeof(decoded));

    int decoded_len = base64_decode(encoded, decoded);
    decoded[decoded_len] = '\0'; // Asegura el null terminator

    printf("Decodificado desde Base64: %s\n", decoded);

    return 0;
}
