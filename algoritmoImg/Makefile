# Build target
build: main.o  lsb3.o
	gcc main.c lsb3.c -o algoritmo


# Regla para main.o
main.o: main.c
	gcc -c main.c

# Regla para utils.o
lsb3.o: lsb3.c
	gcc -c lsb3.c

# Limpieza de archivos en Windows
clean:
	del /Q *.o 2>nul || exit 0
	del /Q steganogram.txt 2>nul || exit 0
	del /Q out\*.* 2>nul || exit 0