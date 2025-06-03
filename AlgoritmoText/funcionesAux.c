#include "funcionesAux.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Contante que muestra las letras que formaran parte de nuestra base, quito la Ñ para trabajar de forma mas universal y unificada.
const char base16_letras[] = "EAOSRNIDLCTUMPBG";
/*
 * Función: caracter_a_base16
 * Descripción: Funcion auxiliar privada para calcular el valor correspondiente en nuestra base convirtiendo un numero de 4 bits en dicha representacion
 *
 * Parámetros:
 * - int valor: entero que representa el numero en binario
 *
 * Valor de retorno:
 * - char: Devuelve el caracter que corresponde en nuestra base.
 *
 */
static char caracter_a_base16(int valor)
{
    if (valor >= 0 && valor < 16)
    {
        return (base16_letras[valor]);
    }
    else
    {
        fprintf(stderr, "ERROR. convertir a Base 16 el caracter. Metodo: caracter_a_base16()\n");
        exit(1);
    }
}
/*
 * Función: tratar_binario
 * Descripción: Devuelve el valor correspondiente en nuestra base
 *
 * Parámetros:
 * - char c: indica la letra del mensaje
 *
 * Valor de retorno:
 * - char: Devuelve una cadena de dos caracteres que corresponde en nuestra base.
 *
 * Errores:
 * - Retorna NULL si hay un error en la asignación de memoria.
 */
static char *tratar_binario(char c)
{

    // Seleccionamos los 4 bits más significativos (MSB)
    // Le aplicamos una AND al valor desplazado contra la cadena 00001111
    int parte1 = (c >> 4) & 0x0F;

    // Seleccionamos los 4 bits menos significativos (LSB)
    int parte2 = c & 0x0F;

    // Reservar espacio para los dos caracteres mas el '/0' que indica el final de la cadena
    char *cadRes = (char *)malloc(3 * sizeof(char));

    if (cadRes == NULL)
    {
        fprintf(stderr, "ERROR. Asignar la memoria. Método: tratar_binario()\n");
        exit(1);
    }

    // Convertir cada cadena a base 16 y almacenarlo en la cadena resultado
    cadRes[0] = caracter_a_base16(parte1);
    cadRes[1] = caracter_a_base16(parte2);
    cadRes[2] = '\0';

    return cadRes; // Devuelve la cadena resultado que contiene los dos caracteres que codifican el primer caracter del mensaje.
}
/*
 * Función: entero_binario
 * Descripción: Devuelve el valor correspondiente a un entero en su representacion binaria
 *
 * Parámetros:
 * - int valor: valor numerico decimal
 *
 * Valor de retorno:
 * - char: Devuelve el carracter que corresponde en nuestra base.
 *
 * Errores:
 * - Retorna NULL si hay un error en la asignación de memoria.
 */
static char *entero_binario(int n)
{
    // Número de bits en un entero
    int bits = sizeof(int);

    // Reserva memoria para la cadena de caracteres (bits + 1 para el terminador nulo)
    char *binaryString = (char *)malloc(bits + 1);
    if (binaryString == NULL)
    {
        // Manejo de errores en caso de que malloc falle
        fprintf(stderr, "ERROR. No se pudo asignar memoria suficiente. Metodo: entero_binario()\n");
        exit(1);
    }
    // Itera sobre cada bit desde el más significativo hasta el menos significativo
    for (int i = 0; i < bits; i++)
    {
        int bit = (n >> (bits - 1 - i)) & 1; // Obtén el bit i-ésimo
        binaryString[i] = bit ? '1' : '0';
    }
    binaryString[bits] = '\0';
    return binaryString;
}
/*
 * Función: indexOfBase
 * Descripción:Busca en el array de letras cual es la posicion que tiene la letra pasada por parametro
 *
 * Parámetros:
 * - char letra: letra que se desea buscar
 *
 * Valor de retorno:
 * - int: Posicion en la que se encuentra, debe devolver siempre un valor entre 0-15
 *
 * Errores:
 * - Retorna -1 si no se ha encontrado
 */
static int indexOfBase(char letra)
{
    int posicion = -1; // Iniciar con -1 para indicar "no encontrado"
    int i = 0;
    int encontrado = 0;
    // Si no se ha llegado al final de la cadena ni encontrado el caracter que buscamos sigue iterando
    while (base16_letras[i] != '\0' && !encontrado)
    {
        if (base16_letras[i] == letra)
        {
            posicion = i; // Termina el bucle cuando se encuentra la letra introducida por parametro
        }
        i++;
    }
    if (posicion < 0)
    {
        fprintf(stderr, "ERROR. No se ha encontrado el indice del caracter. Metodo: indexOfBase()\n");
        exit(1);
    }
    return posicion;
}
/*
 * Función: imprimirCadena
 * Descripción: Muestra por pantalla la cadena de caracteres que se le pasa por parametro
 *
 * Parámetros:
 * - const char *cadena: una cadena de caracteres
 *
 * Valor de retorno:
 * - Como es de tipo void, en este caso no retorna ningun valor
 *
 */
void imprimirCadena(const char *cadena)
{
    printf("%s\n", cadena);
}
/*
 * Función: binario_a_ascii
 * Descripción: Esta funcion se encarga de calcular el valor numerico de una cadena de 8 bits.
 *
 * Parámetros:
 * - const char *cadena_binaria:
 *
 * Valor de retorno:
 * - char: Devuelve el caracter ASCII correspondiente.
 *
 * Errores:
 * - Devuelve error si el valor que deseamos devolver como ASCII no se encuentra entre 32-126 que son los caracteres imprimibles
 */
static char binario_a_ascii(const char *cadena_binaria)
{
    // Como la recorremos de derecha a izquierda empezamos con el peso mayor y vamos diviendo este en cada iteraccion
    int peso = 128;
    int valor_ascii = 0;

    // Asegúrate de que la cadena tenga 8 bits
    if (strlen(cadena_binaria) != 8)
    {
        fprintf(stderr, "ERROR. La cadena binaria debe tener 8 bits. Metodo binario_a_ascii()\n");
        return '\0';
    }

    // Convertir la cadena binaria a un valor entero
    for (int i = 0; i < 8; i++)
    {
        if (cadena_binaria[i] == '1')
        {
            valor_ascii += peso; // Se suma a lo acumulado el peso que tengamos en este punto.
        }
        peso /= 2;
    }
    // Se comprueba que el valor calculado este entre los caracteres imprimibles ASCII
    if (valor_ascii < 32 && valor_ascii > 126)
    {
        fprintf(stderr, "ERROR. Cuando se ha realizado la conversion a ascii a un caracter imprimible. Metodo binario_a_ascii()\n");
        exit(1);
    }
    //   Convertir el valor nunmerico que obtenemos de la cadena binaria a su carácter ASCII realoizando un casting
    return (char)valor_ascii;
}

char *codificarMsg(const char *contenidoArchivo)
{

    // Ahora procesamos el contenido para convertirlo a binario
    char *usar_letras = (char *)malloc(1);
    if (usar_letras == NULL)
    {
        printf("ERROR. Al asignar memoria. Metodo: codificarMsg()\n");
        return NULL;
    }
    usar_letras[0] = '\0';

    for (size_t i = 0; i < strlen(contenidoArchivo); i++)
    {
        char c = contenidoArchivo[i];
        char *letras_binarias = tratar_binario(c);
        usar_letras = (char *)realloc(usar_letras, strlen(usar_letras) + strlen(letras_binarias) + 1);
        strcat(usar_letras, letras_binarias);
        free(letras_binarias);
    }

    return usar_letras;
}

char *decodificarMsg(char *cadenaOculta)
{
    size_t capacidad = 10;
    char *mensajeDecodificado = (char *)malloc(capacidad * sizeof(char));
    if (mensajeDecodificado == NULL)
    {
        printf("ERROR. Al asignar memoria. Metodo: decodificarMsg()\n");
        return NULL;
    }

    size_t k = 0; // Índice para mensajeDecodificado

    for (int i = 0; cadenaOculta[i] != '\0'; i += 2)
    {
        char cadena[9]; // Asume que entero_binario retorna una cadena de 4 bits.

        // Concatenación manual
        strcpy(cadena, entero_binario(indexOfBase(cadenaOculta[i])));
        strcat(cadena, entero_binario(indexOfBase(cadenaOculta[i + 1])));

        char cad = binario_a_ascii(cadena);

        // Aumentar capacidad si es necesario
        if (k >= capacidad - 1)
        {                   // -1 para dejar espacio para el terminador null
            capacidad *= 2; // Doble capacidad
            char *nuevoBuffer = (char *)realloc(mensajeDecodificado, capacidad * sizeof(char));
            if (nuevoBuffer == NULL)
            {
                free(mensajeDecodificado);
                perror("Error al reasignar memoria");
                return NULL;
            }
            mensajeDecodificado = nuevoBuffer;
        }

        mensajeDecodificado[k++] = cad;
    }

    mensajeDecodificado[k] = '\0'; // Terminar la cadena con un carácter nulo
    return mensajeDecodificado;
}

char *extraerMayusculas(const char *cadena)
{

    // Crear un buffer para la cadena resultante
    // Como no sabemos cuántas letras mayúsculas hay, asumimos que todas podrían serlo (por lo que asignamos longitud+1)
    char *mayusculas = (char *)malloc((strlen(cadena) + 1) * sizeof(char));
    if (mayusculas == NULL)
    {
        printf("ERROR. Al asignar memoria. Metodo: extraerMayusculas()\n");
        return NULL;
    }

    int j = 0; // Índice para la cadena resultante

    // Recorrer la cadena original
    for (int i = 0; i < strlen(cadena); i++)
    {
        // Si el carácter es mayúscula, lo añadimos a la cadena resultante
        if (isupper(cadena[i]))
        {
            mayusculas[j++] = cadena[i];
        }
    }

    // Terminar la cadena resultante con un carácter nulo
    mayusculas[j] = '\0';

    return mayusculas;
}