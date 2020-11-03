.PHONY: clean test all e2e

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -pedantic -g -std=c99 -march=native

BIN_TEST:=./microktc_debug

all: microktc microktc_debug e2e

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -DNDEBUG -O2 $(SRC) -o $@

microktc_debug: $(SRC) $(HEADERS) macos_x86_64_stdlib.h
	$(CC) $(CFLAGS) -O0 -fsanitize=address $(SRC) -o $@

macos_x86_64_stdlib.h: macos_x86_64_stdlib.asm stdlib_asm_to_h.awk
	awk -f stdlib_asm_to_h.awk $< > $@

clean:
	rm -rf microktc microktc_debug ./*.dSYM e2e/*.o e2e/*.asm e2e/print_bool e2e/print_string e2e/print_integers e2e/hello_world

e2e/print_bool: e2e/print_bool.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/print_string: e2e/print_string.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/print_integers: e2e/print_integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/hello_world: e2e/hello_world.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e: e2e/print_bool e2e/print_string e2e/print_integers e2e/hello_world

test: e2e tests.awk
	@./tests.awk
