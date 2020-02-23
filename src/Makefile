CC       = gcc

CFLAGS   = -O2 -Wall
LDFLAGS   = -pthread

SRC      = $(wildcard *.c)
EXEC     = $(SRC:.c=)

all: $(EXEC)

$(EXEC): $(SRC)
	$(CC) -o $@ $@.c $(CFLAGS) $(LDFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf *.o
	@rm -rf $(EXEC)
