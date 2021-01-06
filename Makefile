.POSIX:
.PHONY: clean check install

SRC = main.c
HEADERS := $(wildcard *.h)

DESTDIR ?= /usr/local/

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
else ifeq "$(strip $(OS))" "Linux"
	ASAN_DIR=$(shell $(CC) -print-search-dirs | awk -F '=' '/libraries/{split($$2, libs, ":"); printf("%s/lib/linux", libs[1])}')
	WITH_DTRACE = 0
else
	$(error Unsupported OS, see README)
endif

CFLAGS_COMMON = -Wall -Wextra -pedantic -Wno-dollar-in-identifier-extension -g -std=c99 -march=native -fno-omit-frame-pointer -fstrict-aliasing -fPIC -D_POSIX_C_SOURCE=200809 -fno-stack-protector
CFLAGS = $(CFLAGS_COMMON) -DLD='"$(LD)"' -DAS='"$(AS)"'
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

TESTS_SRC := $(wildcard tests/*.kt)
TESTS_O := $(TESTS_SRC:.kt=.o)
TESTS_ASM := $(TESTS_SRC:.kt=.asm)
TESTS_EXE := $(TESTS_SRC:.kt=.exe)

.DEFAULT:
mktc: $(SRC) $(HEADERS) mkt_stdlib.o
	$(CC) $(CFLAGS) $(SRC) -o $@


mkt_stdlib.o: mkt_stdlib.c probes.h
	$(CC) $(CFLAGS_STDLIB) $< -c

test: test.c
	$(CC) $(CFLAGS) $< -o $@

probes.h: probes.d
ifeq "$(WITH_DTRACE)" "1"
		dtrace -o $@ -h -s $<
endif

%.exe: %.kt mktc
	./mktc $<

check: mktc $(TESTS_EXE) test
	@./test

clean:
	$(RM) mktc {tests,err}/*.{asm,o,exe} mkt_stdlib.o
	$(RM) -r ./*.dSYM

install: mktc
	mkdir -p $(DESTDIR)/lib/
	mkdir -p $(DESTDIR)/bin/
	cp mkt_stdlib.o $(DESTDIR)/lib/mkt_stdlib.o
	cp mktc $(DESTDIR)/bin/mktc
