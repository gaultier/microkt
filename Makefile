.PHONY: clean

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -Wpedantic -Wno-format-pedantic -g -std=c99 -march=native

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O2 $(SRC) -o $@

microktc_debug: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O0 -fsanitize=address $(SRC) -o $@

clean:
	rm -rf microktc microktc_debug ./*.dSYM
