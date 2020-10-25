.PHONY: clean

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -Wpedantic -g -O2 -std=c99

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@

microktc_debug: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -fsanitize=address $(SRC) -o $@

clean:
	rm microktc microktc_debug
