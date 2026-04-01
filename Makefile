CC = cc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Isrc
SRC = src/main.c src/cpu/cpu.c src/cpu/cpu_ops.c src/cpu/cpu_table.c src/bus/bus.c src/cart/cart.c
OUT = knes

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

nestest: $(OUT)
	./$(OUT) test/roms/nestest.nes --nestest > test/logs/nestest_out.log 2>/dev/null
	python3 test/extract_nestest.py test/roms/nestest.log > test/logs/nestest_ref.txt
	diff -u test/logs/nestest_ref.txt test/logs/nestest_out.log
	@echo "NESTEST PASSED"
clean:
	rm -f $(OUT) test/logs/nestest_out.log test/logs/nestest_ref.txt

.PHONY: clean nestest
