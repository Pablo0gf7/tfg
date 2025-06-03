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

#endif // funcionesAux_h