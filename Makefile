CC = cc
CFLAGS = -Wall -Wextra -std=c99 -O2
SRC = src/main.c src/cpu.c src/bus.c
OUT = knes

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

nestest: $(OUT)
	./$(OUT) test/roms/nestest.nes --nestest > test/logs/nestest_out.log 2>/dev/null
	@# Extract PC + registers + CYC from reference log
	python3 test/extract_nestest.py test/roms/nestest.log > test/logs/nestest_ref.txt
	diff -u test/logs/nestest_ref.txt test/logs/nestest_out.log | head -40

clean:
	rm -f $(OUT) test/logs/nestest_out.log test/logs/nestest_ref.txt

.PHONY: clean nestest
