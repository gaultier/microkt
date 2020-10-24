.PHONY: clean

SRC:=$(wildcard *.{h,c})
CFLAGS+=-Wall -Wextra -Wpedantic -g -O2

microktc: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	rm microktc
