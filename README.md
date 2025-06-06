# TFG

Ejemplos para ejecutar

Compilar el programa
```gcc src/*.c -Iheaders -lsndfile -lfftw3 -lm -lssl -lcrypto -lsodium -o steganografia   ```

## Imagenes

Se guarda directamente en ./out la imagen
- ```./steganografia.exe -i -c resources/image/acordeon.jpg  "Hola" embed.jpg clave123```


- ```./steganografia.exe -i -e ./out/embed.jpg clave123```

## Texto

Falta añadir el cifrado despues de unificar todos y que se guarde la salida en ./out
- ```./steganografia.exe -t -c ./resources/messages/mensaje2.txt ./resources/texts/texto1000pal.txt```


- ```./steganografia.exe -t -e ./out/steganogram.txt```

## Audio

- ``` ./steganografia.exe -a embed ./resources/audio/pista2.wav  ./out/embed.wav "Hola este es el mensaje"```

Que el fichero con el que se guarda sea en ./out
- ``` ./steganografia.exe -a extract  ./out/embed.wav 40```


# Añadir
- QUe aparte de mostrarse por consola se guarden en un fichero en todos los casos, el cifrado siempre

- Revisar bien el main