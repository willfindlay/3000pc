CC       = gcc

CFLAGS   = -O3 -Wall -Wextra -Wstrict-aliasing
LDFLAGS   = -pthread

SRC      = $(wildcard *.c)
EXEC     = $(SRC:.c=)

all: $(EXEC)

$(EXEC): $(SRC)
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf *.o
	@rm -rf $(EXEC)
