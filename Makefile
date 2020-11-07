.PHONY: clean test all e2e

SRC:=$(wildcard *.c)
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -pedantic -g -std=c99 -march=native

BIN_TEST:=./microktc_debug

all: microktc microktc_debug e2e

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O2 $(SRC) -o $@

microktc_debug: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O0 -fsanitize=address -DWITH_LOGS $(SRC) -o $@

clean:
	rm -rf microktc microktc_debug ./*.dSYM e2e/*.o e2e/*.asm e2e/print_bool e2e/print_string e2e/print_integers e2e/hello_world e2e/print_char e2e/math_integers

# e2e/print_bool: e2e/print_bool.kts $(BIN_TEST)
# 	$(BIN_TEST) $<

# e2e/print_string: e2e/print_string.kts $(BIN_TEST)
# 	$(BIN_TEST) $<

e2e/print_integers: e2e/print_integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

# e2e/print_char: e2e/print_char.kts $(BIN_TEST)
# 	$(BIN_TEST) $<

# e2e/hello_world: e2e/hello_world.kts $(BIN_TEST)
# 	$(BIN_TEST) $<

e2e/math_integers: e2e/math_integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e: e2e/print_integers e2e/math_integers #e2e/hello_world e2e/print_char e2e/print_bool e2e/print_string 

test: e2e tests.awk
	@./tests.awk
