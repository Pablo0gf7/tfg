#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sndfile.h>
#include "../headers/utils.h"

#include "../headers/charReplacement.h"

#include "../headers/lsb2-random.h"

#include "../headers/qim.h"

static char *read_line(char *dst, size_t n, const char *prompt) {
    if (prompt) fputs(prompt, stdout);
    if (!fgets(dst, n, stdin)) return NULL;
    dst[strcspn(dst, "\n")] = 0;
    return dst;
}

static void flush_stdin(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
}

static void print_global_usage(void) {
    printf(
    "Uso:\n"
    "  steganografia [-h|--help] [--version]\n"
    "\n"
    "Subcomandos:\n"
    "  text   embed   -m <mensaje.txt> -b <base.txt> [-o steganogram.txt]\n"
    "  text   extract -i <steganogram.txt> [-o out.txt]\n"
    "\n"
    "  image  embed   -i <in.png> -o <out.png> -k <clave> (-m \"msg\" | -f msg.txt)\n"
    "  image  extract -i <in.png> -k <clave> [-o out.txt]\n"
    "\n"
    "  audio  embed   -i <in.wav> -o <out.wav> -p <password> (-m \"msg\" | -f msg.txt)\n"
    "  audio  extract -i <stego.wav> -p <password>\n"
    "\n"
    "Ejemplos:\n"
    "  ./steganografia text embed -m ./resources/messages/mensajeEjemploMemoria.txt -b ./resources/texts/textEjemploMemoria.txt -o ./out/text_message.txt\n"
    "./steganografia text extract -i ./out/text_message.txt -o ./out/text_message_out.txt\n"

    "./steganografia image embed -i ./resources/image/acordeon.jpg -o acordeon_out.jpg -k clave123 -m Hola\n"
    "./steganografia image embed -i ./resources/image/acordeon.jpg -o acordeon_out.jpg -k clave123 -f ./resources/messages/mensaje4.txt\n" 
    "./steganografia image extract -i ./out/acordeon_out.jpg -k clave123 -o ./out/acordeon_out.txt\n"

    "./steganografia audio embed -i ./resources/audio/space-piano.wav -o ./out/space_piano_out.wav -p clave123 -m Algoritmo123\n"
    "./steganografia audio embed -i ./resources/audio/space-piano.wav -o ./out/space_piano_out.wav -p clave123 -f ./resources/messages/mensajeEjemploMemoria.txt \n"
    "./steganografia audio extract -i  ./out/space_piano_out.wav -p clave123\n"
    );
}




int main(int argc, char **argv) {
    if (argc == 1) { print_global_usage(); return 0; }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        print_global_usage(); return 0;
    }
    if (!strcmp(argv[1], "--version")) {
        puts("steganografia 1.0"); return 0;
    }


    if (!strcmp(argv[1], "text")) {
        if (argc >= 3 && !strcmp(argv[2], "embed")) {
            const char *m = NULL, *b = NULL, *o = NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-m") && i + 1 < argc) m = argv[++i];
                else if (!strcmp(argv[i], "-b") && i + 1 < argc) b = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) o = argv[++i];
            }
            return text_embed_cli(m, b, o);
        } else if (argc >= 3 && !strcmp(argv[2], "extract")) {
            const char *iPath = NULL, *o = NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) iPath = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) o = argv[++i];
            }
            return text_extract_cli(iPath, o);
        } else {
            print_global_usage(); return 1;
        }
    } else if (!strcmp(argv[1], "image")) {
        if (argc >= 3 && !strcmp(argv[2], "embed")) {
            const char *in=NULL,*out=NULL,*key=NULL,*msg=NULL,*file=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
                else if (!strcmp(argv[i], "-k") && i + 1 < argc) key = argv[++i];
                else if (!strcmp(argv[i], "-m") && i + 1 < argc) msg = argv[++i];
                else if (!strcmp(argv[i], "-f") && i + 1 < argc) file = argv[++i];
            }
            char *message_buf = NULL;
            if (!msg && file) {
                FILE *fm = abrirArchivo(file, "rb");
                if (!fm) { fprintf(stderr, "No se pudo abrir %s\n", file); return 1; }
                message_buf = leerArchivo(fm);
                cerrarArchivo(fm);
                msg = message_buf;
            }
            if (!in || !out || !key || !msg) { fprintf(stderr, "Parámetros requeridos: -i -o -k (-m | -f)\n"); free(message_buf); return 1; }
            embedMessage(in, msg, out, key);
            free(message_buf);
            return 0;
        } else if (argc >= 3 && !strcmp(argv[2], "extract")) {
            const char *in=NULL,*key=NULL,*out=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-k") && i + 1 < argc) key = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
            }
            if (!in || !key) { fprintf(stderr, "Parámetros requeridos: -i -k [-o]\n"); return 1; }
            char *m = extractMessage(in, key);
            if (!m) { fprintf(stderr, "No se pudo extraer el mensaje.\n"); return 2; }
            if (out) {
                FILE *fo = abrirArchivo(out, "wb");
                if (!fo) { fprintf(stderr, "No se pudo abrir %s\n", out); free(m); return 1; }
                if (escribirArchivo(fo, m) != 0) { fprintf(stderr, "Error al escribir %s\n", out); cerrarArchivo(fo); free(m); return 1; }
                cerrarArchivo(fo);
                printf("Mensaje -> %s\n", out);
            } else {
                printf("%s\n", m);
            }
            free(m);
            return 0;
        } else {
            print_global_usage(); return 1;
        }
    } else if (!strcmp(argv[1], "audio")) {
        if (argc >= 3 && !strcmp(argv[2], "embed")) {
            const char *in=NULL,*out=NULL,*pass=NULL,*msg=NULL,*file=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
                else if (!strcmp(argv[i], "-p") && i + 1 < argc) pass = argv[++i];
                else if (!strcmp(argv[i], "-m") && i + 1 < argc) msg = argv[++i];
                else if (!strcmp(argv[i], "-f") && i + 1 < argc) file = argv[++i];
            }
            char *message_buf = NULL;
            if (!msg && file) {
                FILE *fm = abrirArchivo(file, "rb");
                if (!fm) { fprintf(stderr, "No se pudo abrir %s\n", file); return 1; }
                message_buf = leerArchivo(fm);
                cerrarArchivo(fm);
                msg = message_buf;
            }
            if (!in || !out || !pass || !msg) { fprintf(stderr, "Parámetros requeridos: -i -o -p (-m | -f)\n"); free(message_buf); return 1; }
            embed_message(in, out, msg, pass);
            free(message_buf);
            return 0;
        } else if (argc >= 3 && !strcmp(argv[2], "extract")) {
            const char *in=NULL,*pass=NULL,*out=NULL;
            for (int i = 3; i < argc; ++i) {
                if (!strcmp(argv[i], "-i") && i + 1 < argc) in = argv[++i];
                else if (!strcmp(argv[i], "-p") && i + 1 < argc) pass = argv[++i];
                else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
            }
            if (!in || !pass) { fprintf(stderr, "Parámetros requeridos: -i -p [-o]\n"); return 1; }
            if (out) {
                fprintf(stderr, "(Nota) extract_message imprime por pantalla; captura manual a %s si lo necesitas.\n", out);
            }
            extract_message(in, pass);
            return 0;
        } else {
            print_global_usage(); return 1;
        }
    } else {
        print_global_usage(); return 1;
    }
}
