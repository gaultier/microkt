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

TESTS_SRC := $(wildcard test/*.kts)
TESTS_O := $(TESTS_SRC:.kts=.o)
TESTS_ASM := $(TESTS_SRC:.kts=.asm)
TESTS_EXE := $(TESTS_SRC:.kts=.exe)
TESTS_ACTUAL := $(TESTS_SRC:.kts=.actual)
TESTS_EXPECTED := $(TESTS_SRC:.kts=.expected)
TESTS_DIFF := $(TESTS_SRC:.kts=.diff)

.DEFAULT:
microktc: $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@

.SUFFIXES: .kts .exe .actual .expected

.kts.exe: microktc $(TESTS_SRC)
	./microktc $<

.exe.actual: microktc $(TESTS_SRC) $(TESTS_EXE) $(TESTS_ACTUAL)
	./$< > $@

.kts.expected: microktc $(TESTS_SRC) $(TESTS_EXPECTED)
	@awk -F '// expect: ' '/expect: / {print $$2} ' $< > $@

test: microktc $(TESTS_SRC) $(TESTS_ACTUAL) $(TESTS_EXPECTED) test.sh
	@./test.sh

clean:
	rm -f microktc $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O) $(TESTS_ACTUAL) $(TESTS_EXPECTED) $(TESTS_DIFF)
	rm -rf ./*.dSYM
