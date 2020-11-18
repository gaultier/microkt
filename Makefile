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
TESTS_O = $(TESTS_SRC:.kts=.o)
TESTS_ASM = $(TESTS_SRC:.kts=.asm)
TESTS_EXE = $(TESTS_SRC:.kts=.exe)

$(BIN): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@

clean:
	rm -f $(BIN) $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O)
	rm -rf ./*.dSYM

.SUFFIXES: .kts .exe

.kts.exe: $(BIN)
	./$(BIN) $<

test: $(TESTS_EXE) tests.awk $(BIN)
	@./tests.awk
