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
	rm -rf microktc microktc_debug ./*.dSYM
	find e2e -not -name '*.kts' -delete

e2e/bool: e2e/bool.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/string: e2e/string.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/integers: e2e/integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/char: e2e/char.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/hello_world: e2e/hello_world.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/math_integers: e2e/math_integers.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/comparison: e2e/comparison.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/negation: e2e/negation.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e/if: e2e/if.kts $(BIN_TEST)
	$(BIN_TEST) $<

e2e: e2e/integers e2e/math_integers e2e/char e2e/hello_world e2e/string e2e/bool e2e/comparison e2e/negation e2e/if

test: e2e tests.awk
	@./tests.awk
