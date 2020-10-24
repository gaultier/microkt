.PHONY: clean

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -Wpedantic -g -O2 -std=c99

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	rm microktc
