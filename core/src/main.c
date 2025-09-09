#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//#include <sndfile.h> // No se usa aquí; los módulos de audio lo incluyen donde toca

#include "../headers/utils.h"
#include "../headers/charReplacement.h"
#include "../headers/lsb.h"
#include "../headers/qim.h"

/**
 * @file main.c
 * @brief CLI unificado para esteganografía en texto, imagen (LSB2) y audio.
 *
 * @details
 *  Subcomandos soportados:
 *   - text   embed   -m <mensaje.txt> -b <base.txt> [-o steganogram.txt]
 *   - text   extract -i <steganogram.txt> [-o out.txt]
 *   - image  embed   -i <in.png> -o <out.png> -k <clave> (-m "msg" | -f msg.txt)
 *   - image  extract -i <in.png> -k <clave> [-o out.txt]
 *   - audio  embed   -i <in.wav> -o <out.wav> -k <password> (-m "msg" | -f msg.txt)
 *   - audio  extract -i <stego.wav> -k <password> [-o out.txt]
 *
 *  Mejoras respecto a la versión previa:
 *   - Validación de combinaciones exclusivas (-m vs -f).
 *   - Lectura desde stdin si se pasa "-m -" (útil para pipes).
 *   - Mensajería de error coherente a stderr y códigos de salida estándar.
 *   - `print_global_usage()` usa el nombre real del binario.
 *   - Limpieza de includes no usados.
 *
 *  Nota: `audio extract` actualmente llama a `extract_message()` que imprime en stdout. Si se pasa `-o`,
 *  se recomienda refactorizar `extract_message()` para que devuelva un buffer como hace `extractMessage()` de imagen.
 */

static const char *progname = "steganografia"; // actualizado en main() con argv[0]

// --------------------------- Utilidades CLI internas ---------------------------

static void print_global_usage(void) {
    fprintf(stdout,
    "Uso:\n"
    "  %s [-h|--help] [--version]\n"
    "\n"
    "Subcomandos:\n"
    "  text   embed   -m <mensaje.txt> -b <base.txt> [-o steganogram.txt]\n"
    "  text   extract -i <steganogram.txt> [-o out.txt]\n"
    "\n"
    "  image  embed   -i <in.png> -o <out.png> -k <clave> (-m \"msg\" | -f msg.txt)\n"
    "  image  extract -i <in.png> -k <clave> [-o out.txt]\n"
    "\n"
    "  audio  embed   -i <in.wav> -o <out.wav> -k <password> (-m \"msg\" | -f msg.txt)\n"
    "  audio  extract -i <stego.wav> -k <password> [-o out.txt]\n"
    "\n"
    "Ejemplos:\n"
    "  %s text embed   -m ./resources/messages/mensajeEjemploMemoria.txt \\\n -b ./resources/texts/textEjemploMemoria.txt -o ./out/text_message.txt\n"
    "  %s text extract -i ./out/text_message.txt -o ./out/text_message_out.txt\n"
    "\n"
    "  %s image embed  -i ./resources/image/acordeon.jpg -o acordeon_out.jpg -k clave123 -m Hola\n"
    "  %s image embed  -i ./resources/image/acordeon.jpg -o acordeon_out.jpg -k clave123 -f ./resources/messages/mensaje4.txt\n"
    "  %s image extract -i ./out/acordeon_out.jpg -k clave123 -o ./out/acordeon_out.txt\n"
    "\n"
    "  %s audio embed  -i ./resources/audio/space-piano.wav -o ./out/space_piano_out.wav -k clave123 -m Algoritmo123\n"
    "  %s audio embed  -i ./resources/audio/space-piano.wav -o ./out/space_piano_out.wav -k clave123 -f ./resources/messages/mensajeEjemploMemoria.txt\n"
    "  %s audio extract -i ./out/space_piano_out.wav -k clave123\n",
    progname, progname, progname, progname, progname, progname, progname, progname,progname);
}

/**
 * @brief Lee un mensaje desde los flags -m/-f. Enforce exclusividad. Permite -m "-" para stdin.
 * @param m  cadena literal (puede ser "-")
 * @param f  ruta de fichero
 * @param out_msg salida (heap). Debe liberarse con free().
 * @return 0 ok, !=0 error ya impreso.
 */
static int read_msg_from_flags(const char *m, const char *f, char **out_msg)
{
    *out_msg = NULL;
    if (m && f) { fprintf(stderr, "Error: use -m o -f, pero no ambos.\n"); return 1; }
    if (!m && !f) { fprintf(stderr, "Error: falta el mensaje (-m \"...\" | -f fichero).\n"); return 1; }

    if (m) {
        if (strcmp(m, "-") == 0) {
            // Leer de stdin hasta EOF
            size_t cap = 4096, len = 0; char *buf = (char*)malloc(cap);
            if (!buf) { perror("malloc"); return 1; }
            int c;
            while ((c = fgetc(stdin)) != EOF) {
                if (len + 1 >= cap) { cap *= 2; char *nb = (char*)realloc(buf, cap); if (!nb) { free(buf); perror("realloc"); return 1; } buf = nb; }
                buf[len++] = (char)c;
            }
            buf[len] = '\0';
            *out_msg = buf;
            return 0;
        } else {
            // Copia directa del literal -m (heap)
            size_t L = strlen(m) + 1; char *buf = (char*)malloc(L);
            if (!buf) { perror("malloc"); return 1; }
            memcpy(buf, m, L);
            *out_msg = buf;
            return 0;
        }
    }
    // m es NULL -> leer de fichero f
    FILE *fm = abrirArchivo(f, "rb");
    if (!fm) { fprintf(stderr, "No se pudo abrir %s\n", f); return 1; }
    char *msg = leerArchivo(fm);
    cerrarArchivo(fm);
    if (!msg) { fprintf(stderr, "No se pudo leer %s\n", f); return 1; }
    *out_msg = msg;
    return 0;
}

// ------------------------------------ main ------------------------------------

int main(int argc, char **argv) {
    if (argc > 0 && argv[0] && *argv[0]) progname = argv[0];

    if (argc == 1) { print_global_usage(); return EXIT_SUCCESS; }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        print_global_usage(); return EXIT_SUCCESS;
    }
    if (!strcmp(argv[1], "--version")) {
        puts("steganografia 1.0"); return EXIT_SUCCESS;
    }

    // ------------------------------ Texto ------------------------------
    if (!strcmp(argv[1], "text")) {
        if (argc >= 3 && !strcmp(argv[2], "embed")) {
            const char *m = NULL, *b = NULL, *o = NULL, *f = NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-m") && i + 1 < argc) m = argv[++i];
                else if (!strcmp(argv[i], "-b") && i + 1 < argc) b = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) o = argv[++i];
            }
            if (!b) { fprintf(stderr, "Falta -b <base.txt>\n"); return EXIT_FAILURE; }
            char *msg = NULL; if (read_msg_from_flags(m, f, &msg) != 0) return EXIT_FAILURE;
            int rc;
            if(m!=NULL)  rc = text_embed_cli(m, b, o); 
            
            free(msg);
            return rc;
        } else if (argc >= 3 && !strcmp(argv[2], "extract")) {
            const char *iPath = NULL, *o = NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) iPath = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) o = argv[++i];
            }
            return text_extract_cli(iPath, o);
        } else {
            print_global_usage(); return EXIT_FAILURE;
        }
    }

    // ------------------------------ Imagen ------------------------------
    else if (!strcmp(argv[1], "image")) {
        if (argc >= 3 && !strcmp(argv[2], "embed")) {
            const char *in=NULL,*out=NULL,*key=NULL,*m=NULL,*f=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
                else if (!strcmp(argv[i], "-k") && i + 1 < argc) key = argv[++i];
                else if (!strcmp(argv[i], "-m") && i + 1 < argc) m = argv[++i];
                else if (!strcmp(argv[i], "-f") && i + 1 < argc) f = argv[++i];
            }
            if (!in || !out || !key) { fprintf(stderr, "Parámetros requeridos: -i -o -k (-m | -f)\n"); return EXIT_FAILURE; }
            char *msg = NULL; if (read_msg_from_flags(m, f, &msg) != 0) return EXIT_FAILURE;
            embedMessage(in, msg, out, key);
            free(msg);
            return EXIT_SUCCESS;
        } else if (argc >= 3 && !strcmp(argv[2], "extract")) {
            const char *in=NULL,*key=NULL,*out=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-k") && i + 1 < argc) key = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
            }
            if (!in || !key) { fprintf(stderr, "Parámetros requeridos: -i -k [-o]\n"); return EXIT_FAILURE; }
            char *m = extractMessage(in, key);
            if (!m) { fprintf(stderr, "No se pudo extraer el mensaje.\n"); return 2; }
            if (out) {
                FILE *fo = abrirArchivo(out, "wb");
                if (!fo) { fprintf(stderr, "No se pudo abrir %s\n", out); free(m); return EXIT_FAILURE; }
                if (escribirArchivo(fo, m) != 0) { fprintf(stderr, "Error al escribir %s\n", out); cerrarArchivo(fo); free(m); return EXIT_FAILURE; }
                cerrarArchivo(fo);
                printf("Mensaje -> %s\n", out);
            } else {
                printf("%s\n", m);
            }
            free(m);
            return EXIT_SUCCESS;
        } else {
            print_global_usage(); return EXIT_FAILURE;
        }
    }

    // ------------------------------ Audio ------------------------------
    else if (!strcmp(argv[1], "audio")) {
        if (argc >= 3 && !strcmp(argv[2], "embed")) {
            const char *in=NULL,*out=NULL,*pass=NULL,*m=NULL,*f=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
                else if (!strcmp(argv[i], "-k") && i + 1 < argc) pass = argv[++i];
                else if (!strcmp(argv[i], "-m") && i + 1 < argc) m = argv[++i];
                else if (!strcmp(argv[i], "-f") && i + 1 < argc) f = argv[++i];
            }
            if (!in || !out || !pass) { fprintf(stderr, "Parámetros requeridos: -i -o -k (-m | -f)\n"); return EXIT_FAILURE; }
            char *msg = NULL; if (read_msg_from_flags(m, f, &msg) != 0) return EXIT_FAILURE;
            embed_message(in, out, msg, pass);
            free(msg);
            return EXIT_SUCCESS;
        } else if (argc >= 2 && !strcmp(argv[2], "extract")) {
            const char *in=NULL,*pass=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-k") && i + 1 < argc) pass = argv[++i];
            }
            if (!in || !pass) { fprintf(stderr, "Parámetros requeridos: -i -k [-o]\n"); return EXIT_FAILURE; }
            extract_message(in, pass);
            return EXIT_SUCCESS;
        } else {
            print_global_usage(); return EXIT_FAILURE;
        }
    }

    else {
        print_global_usage(); return EXIT_FAILURE;
    }
}
