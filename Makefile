.PHONY: clean check

SRC = main.c
HEADERS := $(wildcard *.h)

WITH_ASAN = 0
WITH_LOGS=0
DEBUG = 0
ASAN_DIR=$(shell $(CC) -print-search-dirs | awk -F '=' '/libraries/{print $$2}')/lib/darwin/

CFLAGS_ASAN_0 = 
CFLAGS_ASAN_1 = -fsanitize=address -DASAN_DIR='"$(ASAN_DIR)"'
CFLAGS_COMMON =-Wall -Wextra -pedantic -g -std=c99 -march=native -fno-omit-frame-pointer -fstrict-aliasing  -DWITH_LOGS=$(WITH_LOGS) -DWITH_ASAN=$(WITH_ASAN) $(CFLAGS_ASAN_$(WITH_ASAN)) -DLD='"$(LD)"' -DAS='"$(AS)"'
# Debug: build with `make DEBUG=1`
CFLAGS_DEBUG_1 = -O0 
# Release: default
CFLAGS_DEBUG_0 = -O2
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_DEBUG_$(DEBUG))

TESTS_SRC := $(wildcard test/*.kts)
TESTS_O := $(TESTS_SRC:.kts=.o)
TESTS_ASM := $(TESTS_SRC:.kts=.asm)
TESTS_EXE := $(TESTS_SRC:.kts=.exe)
TESTS_ACTUAL := $(TESTS_SRC:.kts=.actual)
TESTS_EXPECTED := $(TESTS_SRC:.kts=.expected)
TESTS_DIFF := $(TESTS_SRC:.kts=.diff)

.DEFAULT:
mktc: $(SRC) $(HEADERS) stdlib.o
	$(CC) $(CFLAGS) $(SRC) -o $@


CFLAGS_STDLIB_ASAN_0 = 
CFLAGS_STDLIB_ASAN_1 = -shared-libasan
stdlib.o: stdlib.c
	$(CC) $(CFLAGS) -DWITH_LOGS=$(WITH_LOGS) $(CFLAGS_ASAN_$(WITH_ASAN)) $< -c

test: test.c
	$(CC) $(CFLAGS) $< -o test

.SUFFIXES: .kts .exe .actual .expected

.kts.exe: mktc $(TESTS_SRC)
	./mktc $<

.exe.actual: mktc $(TESTS_SRC) $(TESTS_EXE) $(TESTS_ACTUAL)
	./$< 2>&1 | grep -v -F '[debug]' > $@

.kts.expected: mktc $(TESTS_SRC) $(TESTS_EXPECTED)
	@awk -F '// expect: ' '/expect: / {print $$2} ' $< > $@

check: mktc $(TESTS_SRC) $(TESTS_ACTUAL) $(TESTS_EXPECTED) test.sh
	@./test.sh

clean:
	rm -f mktc $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O) $(TESTS_ACTUAL) $(TESTS_EXPECTED) $(TESTS_DIFF) stdlib.o stdlib.asm
	rm -rf ./*.dSYM
