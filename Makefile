CC = cc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Isrc
SRC = src/main.c src/nes.c src/cpu/cpu.c src/cpu/cpu_ops.c src/cpu/cpu_table.c src/bus/bus.c src/cart/cart.c src/ppu/ppu.c
OUT = knes

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

nestest: $(OUT)
	./$(OUT) test/nestest/nestest.nes --nestest > test/logs/nestest/out.log 2>/dev/null
	python3 test/nestest/extract.py test/nestest/nestest.log > test/logs/nestest/ref.txt
	diff -u test/logs/nestest/ref.txt test/logs/nestest/out.log
	@echo "NESTEST PASSED"

clean:
	rm -f $(OUT) test/logs/nestest/out.log test/logs/nestest/ref.txt

.PHONY: clean nestest
