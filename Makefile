CC      = gcc
CFLAGS  = -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11
LDFLAGS = -lreadline

SRC     = mini_shell.c main.c
BIN     = shell

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC)
  $(CC) $(CFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

clean:
  rm -f $(BIN)
