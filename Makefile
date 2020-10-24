.PHONY: clean

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -Wpedantic -g -O2

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	rm microktc
