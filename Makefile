.PHONY: clean test

SRC = main.c
HEADERS := $(wildcard *.h)

DEBUG = 0
CFLAGS_COMMON =-Wall -Wextra -pedantic -g -std=c99 -march=native
# Debug: build with `make DEBUG=1`
CFLAGS_1 = -O0 -fsanitize=address -DWITH_LOGS
# Release: default
CFLAGS_0 = -O2
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_$(DEBUG))

BIN = microktc

TESTS_SRC := $(wildcard test/*.kts)
TESTS_O := $(TESTS_SRC:.kts=.o)
TESTS_ASM := $(TESTS_SRC:.kts=.asm)
TESTS_EXE := $(TESTS_SRC:.kts=.exe)
TESTS_ACTUAL := $(TESTS_SRC:.kts=.actual)
TESTS_EXPECTED := $(TESTS_SRC:.kts=.expected)
TESTS_DIFF := $(TESTS_SRC:.kts=.diff)

RED = "\x1b[31m"
GREEN = "\x1b[32m"
RESET = "\x1b[0m"

$(BIN): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	rm -f $(BIN) $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O) $(TESTS_ACTUAL) $(TESTS_EXPECTED) $(TESTS_DIFF)
	rm -rf ./*.dSYM

.SUFFIXES: .kts .exe .actual .expected .diff

.kts.exe: $(BIN)
	@./$(BIN) $<

.exe.actual: $(BIN)
	@./$< > $@

.kts.expected:
	@awk -F '// expect: ' '/expect: / {print $$2} ' $< > $@

.actual.diff:
	@TEST="$<" diff $${TEST/actual/expected} $< > $@ && echo $(GREEN) "✔ " $@ $(RESET) || echo $(RED) "✘ " $@ $(RESET); exit 0

test: $(TESTS_DIFF) $(TESTS_EXPECTED)
