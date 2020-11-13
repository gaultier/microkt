.PHONY: clean test all test tests

SRC:=main.c
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -pedantic -g -std=c99 -march=native

BIN_TEST:=./microktc_debug

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O2 $(SRC) -o $@

all: microktc microktc_debug test

microktc_debug: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O0 -fsanitize=address -DWITH_LOGS $(SRC) -o $@

clean:
	rm -rf microktc microktc_debug ./*.dSYM
	find ./test/ -not -name '*.kts' -delete

test/bool: test/bool.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/string: test/string.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/integers: test/integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/char: test/char.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/hello_world: test/hello_world.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/math_integers: test/math_integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/comparison: test/comparison.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/negation: test/negation.kts $(BIN_TEST)
	$(BIN_TEST) $<

test/if: test/if.kts $(BIN_TEST)
	$(BIN_TEST) $<

tests: test/integers test/math_integers test/char test/hello_world test/string test/bool test/comparison test/negation test/if

test: tests tests.awk
	@./tests.awk
