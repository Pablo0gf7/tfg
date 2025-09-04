/*
 * Archivo: base16_alpha.h
 * Autor: Pablo Garcia Fuentes
 * Fecha: agosto de 2024
 *
 * Descripción:
 * Archivo de cabecera que contiene las declaraciones. Cada metodo se especifica
 * en el fichero base16_alpha.c
 */
#ifndef funcionesAux_h
#define funcionesAux_h
/*
Funciones auxiliares correspondientes con el cambio de base implementado
*/
char *extraerMayusculas(const char *cadena);
char *codificarMsg(const char *nombre_archivo);
char *decodificarMsg(char *cadena);
/*
Funciones auxiliares que simplifican la estructura y facilitan la lectura del codigo
*/
void imprimirCadena(const char *cadena);
static int indexOfBase(char letra);

int text_extract_cli(const char *stego_path, const char *out_path);
int text_embed_cli(const char *msg_path, const char *base_path, const char *out_path);

#endif // funcionesAux_h