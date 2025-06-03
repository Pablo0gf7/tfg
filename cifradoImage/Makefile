# Build target
build: main.o  lsb3.o utils.o
	gcc -o algoritmo main.o lsb3.o utils.o -lsodium



# Regla para main.o
main.o: ./src/main.c
	gcc -c ./src/main.c

# Regla para utils.o
lsb3.o: ./src/lsb3.c
	gcc -c ./src/lsb3.c
utils.o: ./src/utils.c
	gcc -c ./src/utils.c

# Limpieza de archivos en Windows
clean:
	rm *.o 2>nul || exit 0
	rm algoritmo.exe 2>nul || exit 0
	rm ./out/*.* 2>nul || exit 0