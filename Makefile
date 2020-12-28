.POSIX:
.PHONY: clean check

SRC = main.c
HEADERS := $(wildcard *.h)

WITH_ASAN = 0
WITH_LOGS=0
WITH_OPTIMIZE = 0
WITH_DTRACE=1
OS = $(shell uname)
ARCH = $(shell uname -m)
ifneq "$(ARCH)" "x86_64"
	$(error Unsupported architecture, see README)
endif

ifeq "$(strip $(OS))" "Darwin"
	ASAN_DIR=$(shell $(CC) -print-search-dirs | awk -F '=' '/libraries/{print $$2}')/lib/darwin/
else "$(strip $(OS))" "Linux"
	ASAN_DIR=$(shell $(CC) -print-search-dirs | awk -F '=' '/libraries/{split($$2, libs, ":"); printf("%s/lib/linux", libs[1])}')
	WITH_DTRACE ?= 0
else
	$(error Unsupported OS, see README)
endif

CFLAGS_COMMON = -Wall -Wextra -pedantic -Wno-dollar-in-identifier-extension -g -std=c99 -march=native -fno-omit-frame-pointer -fstrict-aliasing -fPIC -D_POSIX_C_SOURCE=200112L
CFLAGS = $(CFLAGS_COMMON) -DLD='"$(CC)"' -DAS='"$(AS)"'
CFLAGS_STDLIB = $(CFLAGS_COMMON)

ifeq "$(WITH_DTRACE)" "0"
	CFLAGS_STDLIB += -DDTRACE_PROBES_DISABLED
endif

ifeq "$(WITH_ASAN)" "1"
	CFLAGS += -fsanitize=address -DASAN_DIR='"$(ASAN_DIR)"' -DWITH_ASAN=1
	CFLAGS_STDLIB += -shared-libasan
else 
	CFLAGS += -DWITH_ASAN=0
	CFLAGS_STDLIB += -DWITH_ASAN=0
endif
	
ifeq "$(WITH_LOGS)" "1"
	CFLAGS += -DWITH_LOGS=1
else
	CFLAGS += -DWITH_LOGS=0
endif

ifeq "$(WITH_OPTIMIZE)" "0"
	CFLAGS += -O0
	CFLAGS_STDLIB += -O0
else
	CFLAGS += -O2
	CFLAGS_STDLIB += -O2
endif

TESTS_SRC := $(wildcard tests/*.kts)
TESTS_O := $(TESTS_SRC:.kts=.o)
TESTS_ASM := $(TESTS_SRC:.kts=.asm)
TESTS_EXE := $(TESTS_SRC:.kts=.exe)

.DEFAULT:
mktc: $(SRC) $(HEADERS) stdlib.o
	$(CC) $(CFLAGS) $(SRC) -o $@


stdlib.o: stdlib.c probes.h
	$(CC) $(CFLAGS_STDLIB) $< -c

test: test.c
	$(CC) $(CFLAGS) $< -o $@

probes.h: probes.d
ifeq "$(WITH_DTRACE)" "1"
		dtrace -o $@ -h -s $<
endif

%.exe: %.kts mktc $(TESTS_SRC)
	./mktc $<

check: mktc $(TESTS_SRC) $(TESTS_EXE) test
	@./test

clean:
	rm -f mktc $(TESTS_EXE) $(TESTS_ASM) $(TESTS_O) stdlib.o stdlib.asm probes.h
	rm -rf ./*.dSYM
