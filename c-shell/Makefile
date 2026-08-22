CC = gcc

CFLAGS = -std=c2x \
         -D_POSIX_C_SOURCE=200809L \
         -D_XOPEN_SOURCE=700 \
         -Wall -Wextra -Werror \
         -Wno-unused-parameter \
         -fno-asm

CPPFLAGS = -Iinclude

TARGET = shell.out

SRC = src/main.c \
      src/prompt.c \
      src/input.c \
      src/lexer.c \
      src/parser.c \
      src/router.c \
      src/shell.c \
      src/hop.c \
      src/peek.c \
      src/reveal.c \
      src/locate.c \
      src/path_utils.c \
      src/arbitrary/arbitrary.c \
      src/arbitrary/command.c \
      src/arbitrary/redirection.c \
      src/arbitrary/pipeline.c
      

OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)