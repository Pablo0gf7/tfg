# Nombre del ejecutable
TARGET = ../dist/stego

# Directorios
SRC_DIR = src
INC_DIR = headers

# Buscar todos los .c del src/
SRCS := $(wildcard $(SRC_DIR)/*.c)

# Compilador y flags
CC = gcc
CFLAGS = -Wall -std=c99 -O2 -I$(INC_DIR)

# Librerías necesarias (orden correcto)
LIBS = -lsodium -lfftw3 -lsndfile -lm 

# Regla principal
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Limpiar binarios
clean:
	rm -f $(TARGET)

# Recompilar desde cero
rebuild: clean all
