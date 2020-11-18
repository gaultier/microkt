.PHONY: clean test all test tests

SRC:=main.c
HEADERS:=$(wildcard *.h)
CFLAGS+=-Wall -Wextra -pedantic -g -std=c99 -march=native

BIN_TEST:=./microktc_debug

TESTS_SRC = $(wildcard test/*.kts)
TESTS_O = $(TESTS_SRC:.kts=.o)
TESTS_ASM = $(TESTS_SRC:.kts=.asm)
TESTS_EXE = $(TESTS_SRC:.kts=.exe)

microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O2 $(SRC) -o $@

all: microktc microktc_debug test

microktc_debug: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -O0 -fsanitize=address -DWITH_LOGS $(SRC) -o $@

clean:
	rm -rf microktc microktc_debug ./*.dSYM $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O)

.SUFFIXES: .kts .exe

.kts.exe: $(BIN_TEST)
	$(BIN_TEST) $<

tests: $(TESTS_EXE)

test: tests tests.awk
	@./tests.awk
