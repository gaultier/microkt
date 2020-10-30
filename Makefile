.PHONY: clean test

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -pedantic -g -std=c99 -march=native

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O2 $(SRC) -o $@

microktc_debug: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O0 -fsanitize=address $(SRC) -o $@

clean:
	rm -rf microktc microktc_debug ./*.dSYM e2e/*.o e2e/*.asm

e2e/print_bool: e2e/print_bool.kts microktc_debug
	./microktc_debug $<

e2e/print_string: e2e/print_string.kts microktc_debug
	./microktc_debug $<

e2e/print_integers: e2e/print_integers.kts microktc_debug
	./microktc_debug $<

test: e2e/print_bool e2e/print_string e2e/print_integers
	@printf "\n===== Tests =====\n"
	./e2e/print_bool
	@printf "\n=====\n"
	./e2e/print_string
	@printf "\n=====\n"
	./e2e/print_integers
	@printf "\n=====\n"
