# Nombre del ejecutable
TARGET = steganografia

# Directorios
SRC_DIR = src
INC_DIR = headers

# Buscar todos los .c del src/
SRCS := $(wildcard $(SRC_DIR)/*.c)

# Compilador y flags
CC = gcc
CFLAGS = -Wall -std=c99 -I$(INC_DIR)
LIBS = -lsndfile -lfftw3 -lm -lssl -lcrypto

# Regla principal
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Limpiar binarios y objetos
clean:
	rm -f $(TARGET)

# Recompilar desde cero
rebuild: clean all
