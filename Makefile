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
	rm -f microktc microktc_debug $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O)
	rm -rf ./*.dSYM

.SUFFIXES: .kts .exe

.kts.exe: microktc_debug
	./microktc_debug $<

test: $(TESTS_EXE) tests.awk microktc_debug
	@./tests.awk
