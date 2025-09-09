#include "../headers/charReplacement.h"
#include "../headers/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * @file charReplacement.c
 * @brief Codificación base-16 con alfabeto de letras frecuentes y esteganografía por capitalización.
 *
 * @details
 *  Mapea cada byte del mensaje a dos símbolos de un alfabeto de 16 letras (nibbles alto/bajo).
 *  Inserta la secuencia en un texto “portador” convirtiendo a MAYÚSCULA cada coincidencia sucesiva.
 *  La extracción toma las mayúsculas en orden y reconstruye los bytes.
 *
 */

// Alfabeto de 16 letras para representar (0..15). Se omite 'Ñ' por portabilidad, y se utilizan las mas utilizadas estadísticamente.
static const char base16_letras[16] = "EAOSRNIDLCTUMPHY";


/**
 * @brief Devuelve la letra correspondiente al valor 0..15.
 */
static inline char nibble_a_letra(unsigned v)
{
    if (v < 16u) return base16_letras[v];
    fprintf(stderr, "ERROR: nibble fuera de rango (%u) en nibble_a_letra()\n", v);
    return '\0';
}

/**
 * @brief Busca el índice 0..15 asociado a una letra del alfabeto. Case-insensitive.
 * @return índice [0,15] o -1 si no pertenece al alfabeto.
 */
static inline int letra_a_nibble(char letra)
{
    unsigned char u = (unsigned char)toupper(letra);
    for (int i = 0; i < 16; ++i) if ((unsigned char)base16_letras[i] == u) return i;
    return -1;
}

// ----------------------------- Funciones públicas ---------------------------

/**
 * @brief Imprime cadena con salto de línea.
 */
void imprimirCadena(const char *cadena)
{
    printf("%s\n", cadena);
}

/**
 * @brief Codifica un buffer de texto en la base16 de letras, 2 símbolos por byte.
 * @param contenidoArchivo Cadena de entrada (UTF-8 tratado byte a byte).
 * @return Nueva cadena (heap) de longitud 2*len + 1. El llamador debe `free()`.
 */
char *codificarMsg(const char *contenidoArchivo)
{
    if (!contenidoArchivo) return NULL;

    size_t len = strlen(contenidoArchivo);
    // Cada byte -> 2 símbolos; +1 para terminador
    size_t outLen = len * 2 + 1;
    char *out = (char *)malloc(outLen);
    if (!out) { perror("malloc"); return NULL; }

    for (size_t i = 0; i < len; ++i) {
        unsigned char uc = (unsigned char)contenidoArchivo[i];
        out[2*i]     = nibble_a_letra(uc >> 4);     // nibble alto
        out[2*i + 1] = nibble_a_letra(uc & 0x0Fu);  // nibble bajo
    }
    out[outLen - 1] = '\0';
    return out;
}

/**
 * @brief Extrae sólo las MAYÚSCULAS de una cadena (para el esteganograma).
 * @return Nueva cadena con las letras mayúsculas (heap). `free()` por el llamador.
 */
char *extraerMayusculas(const char *cadena)
{
    if (!cadena) return NULL;
    size_t n = strlen(cadena);
    char *mayus = (char *)malloc(n + 1);
    if (!mayus) { perror("malloc"); return NULL; }

    size_t j = 0;
    for (size_t i = 0; i < n; ++i)
        if (isupper((unsigned char)cadena[i])) mayus[j++] = cadena[i];

    mayus[j] = '\0';
    return mayus;
}

/**
 * @brief Decodifica una secuencia de letras (longitud par) al mensaje original.
 * @param cadenaOculta Secuencia en alfabeto base16 (típicamente, mayúsculas extraídas).
 * @return Nuevo buffer con el mensaje (heap). `free()` por el llamador.
 */
char *decodificarMsg(char *cadenaOculta)
{
    if (!cadenaOculta) return NULL;
    size_t L = strlen(cadenaOculta);
    if ((L % 2) != 0) { fprintf(stderr, "ERROR: longitud impar en decodificarMsg()\n"); return NULL; }

    size_t outLen = L / 2;
    char *mensaje = (char *)malloc(outLen + 1);
    if (!mensaje) { perror("malloc"); return NULL; }

    for (size_t i = 0, k = 0; i < L; i += 2, ++k) {
        int hi = letra_a_nibble(cadenaOculta[i]);
        int lo = letra_a_nibble(cadenaOculta[i + 1]);
        if (hi < 0 || lo < 0) {
            fprintf(stderr, "ERROR: símbolo fuera de alfabeto en decodificarMsg()\n");
            free(mensaje);
            return NULL;
        }
        mensaje[k] = (char)((hi << 4) | lo);
    }
    mensaje[outLen] = '\0';
    return mensaje;
}

// ------------------------ CLI de inserción/extracción -----------------------

/**
 * @brief Inserta un mensaje en un texto base capitalizando coincidencias sucesivas.
 * @param msg_path   Ruta del fichero con el mensaje en claro.
 * @param base_path  Ruta del texto portador.
 * @param out_path   Ruta del esteganograma (por defecto "steganogram.txt").
 * @return 0 si OK, 1 si error.
 */
int text_embed_cli(const char *msg_path, const char *base_path, const char *out_path)
{
    if (!msg_path || !base_path) { fprintf(stderr, "Faltan parámetros -m y/o -b.\n"); return 1; }

    FILE *fmsg = abrirArchivo(msg_path, "rb");
    if (!fmsg) { fprintf(stderr, "No se pudo abrir %s\n", msg_path); return 1; }
    char *msg = leerArchivo(fmsg); cerrarArchivo(fmsg);
    if (!msg) { fprintf(stderr, "No se pudo leer %s\n", msg_path); return 1; }

    char *msgBase16 = codificarMsg(msg); free(msg);
    if (!msgBase16) { fprintf(stderr, "codificarMsg() falló\n"); return 1; }
    size_t needLen = strlen(msgBase16);

    FILE *fbase = abrirArchivo(base_path, "rb");
    if (!fbase) { fprintf(stderr, "No se pudo abrir %s\n", base_path); free(msgBase16); return 1; }
    char *texto = leerArchivo(fbase); cerrarArchivo(fbase);
    if (!texto) { fprintf(stderr, "No se pudo leer %s\n", base_path); free(msgBase16); return 1; }

    char *textoMinus = convertirMinusculas(texto); free(texto);
    if (!textoMinus) { fprintf(stderr, "convertirMinusculas() falló\n"); free(msgBase16); return 1; }

    size_t L = strlen(textoMinus);

    // --- Pre-chequeo de capacidad por letra ---
    int need[16] = {0}, have[16] = {0};
    for (size_t j = 0; j < needLen; ++j) {
        int idx = letra_a_nibble(msgBase16[j]);
        if (idx >= 0) need[idx]++; else { fprintf(stderr, "Símbolo fuera de alfabeto en msgBase16\n"); free(textoMinus); free(msgBase16); return 1; }
    }
    for (size_t i = 0; i < L; ++i) {
        int idx = letra_a_nibble(textoMinus[i]);
        if (idx >= 0) have[idx]++;
    }
    for (int t = 0; t < 16; ++t) {
        if (have[t] < need[t]) {
            fprintf(stderr, "Capacidad insuficiente para '%c': %d < %d\n", base16_letras[t], have[t], need[t]);
            free(textoMinus); free(msgBase16); return 1;
        }
    }

    // Preparar salida (copia en la que mayusculizamos posiciones)
    char *salida = (char*)malloc(L + 1);
    if (!salida) { perror("malloc"); free(textoMinus); free(msgBase16); return 1; }
    memcpy(salida, textoMinus, L + 1);

    // Embedding secuencial: para cada símbolo, elevar a MAYÚSCULA la siguiente coincidencia en el portador.
    size_t pos = 0;
    for (size_t j = 0; j < needLen; ++j) {
        char targetLower = (char)tolower((unsigned char)msgBase16[j]);
        int found = 0;
        for (; pos < L; ++pos) {
            if (textoMinus[pos] == targetLower) {
                salida[pos] = (char)toupper((unsigned char)salida[pos]);
                ++pos;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Capacidad insuficiente durante embedding para '%c'.\n", msgBase16[j]);
            free(textoMinus); free(msgBase16); free(salida);
            return 1;
        }
    }

    const char *out = (out_path && *out_path) ? out_path : "steganogram.txt";
    FILE *fout = abrirArchivo(out, "wb");
    if (!fout) { fprintf(stderr, "No se pudo abrir salida %s\n", out); free(textoMinus); free(msgBase16); free(salida); return 1; }
    if (escribirArchivo(fout, salida) != 0) { fprintf(stderr, "Error al escribir salida\n"); cerrarArchivo(fout); free(textoMinus); free(msgBase16); free(salida); return 1; }
    cerrarArchivo(fout);

    printf("Esteganograma escrito en %s\n", out);
    free(textoMinus); free(msgBase16); free(salida);
    return 0;
}

/**
 * @brief Extrae el mensaje desde un esteganograma basado en capitalización.
 * @param stego_path Ruta del archivo con el esteganograma.
 * @param out_path   Ruta de salida para el mensaje; si se omite, imprime por stdout.
 * @return 0 si OK, 1 si error.
 */
int text_extract_cli(const char *stego_path, const char *out_path)
{
    if (!stego_path) { fprintf(stderr, "Falta -i <steganogram.txt>\n"); return 1; }
    FILE *fin = abrirArchivo(stego_path, "rb");
    if (!fin) { fprintf(stderr, "No se pudo abrir %s\n", stego_path); return 1; }
    char *contenido = leerArchivo(fin); cerrarArchivo(fin);
    if (!contenido) { fprintf(stderr, "No se pudo leer %s\n", stego_path); return 1; }

    char *mayus = extraerMayusculas(contenido); free(contenido);
    if (!mayus) { fprintf(stderr, "extraerMayusculas() falló\n"); return 1; }

    char *mensaje = decodificarMsg(mayus); free(mayus);
    if (!mensaje) { fprintf(stderr, "decodificarMsg() falló\n"); return 1; }

    if (out_path && *out_path) {
        FILE *fo = abrirArchivo(out_path, "wb");
        if (!fo) { fprintf(stderr, "No se pudo abrir %s\n", out_path); free(mensaje); return 1; }
        if (escribirArchivo(fo, mensaje) != 0) { fprintf(stderr, "Error al escribir %s\n", out_path); cerrarArchivo(fo); free(mensaje); return 1; }
        cerrarArchivo(fo);
        printf("Mensaje extraído -> %s\n", out_path);
    } else {
        printf("Mensaje extraído:\n%s\n", mensaje);
    }
    free(mensaje);
    return 0;
}
