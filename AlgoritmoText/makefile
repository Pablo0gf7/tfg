# Build target
build: main.o funcionesAux.o utils.o
	gcc -o algoritmo funcionesAux.o main.o utils.o

# Regla para funcionesAux.o
funcionesAux.o: funcionesAux.c
	gcc -c funcionesAux.c

# Regla para main.o
main.o: main.c
	gcc -c main.c

# Regla para utils.o
utils.o: utils.c
	gcc -c utils.c

# Limpieza de archivos en Windows
clean:
	del /Q *.o 2>nul || exit 0
	del /Q steganogram.txt 2>nul || exit 0
