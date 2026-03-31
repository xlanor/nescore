CC = cc
CFLAGS = -Wall -Wextra -std=c99 -O2
SRC = src/main.c src/cpu.c src/bus.c
OUT = knes

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -f $(OUT)

.PHONY: clean
