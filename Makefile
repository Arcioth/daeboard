# Default: musl -static. glibc: make STATIC=0
MUSL_GCC := $(shell command -v musl-gcc 2>/dev/null)
CFLAGS ?= -O2 -pipe -Wall -Wextra -std=c11 -s
PREFIX ?= /usr/local
STATIC ?= 1

ifeq ($(STATIC),1)
  ifneq ($(MUSL_GCC),)
    CC := $(MUSL_GCC)
    LDFLAGS += -static
  else
    CC ?= gcc
  endif
else
  CC ?= gcc
endif

SRC = src/main.c src/macro.c src/input.c src/led.c src/ron.c src/sock.c

.PHONY: all test clean

all: daeboard

daeboard: $(SRC) src/gesture.h src/macro.h src/input.h src/led.h src/ron.h src/sock.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC)

tests/gesture_test: tests/gesture_test.c src/gesture.c src/ron.c src/sock.c src/gesture.h src/ron.h src/sock.h
	gcc -std=c11 -Wall -Wextra -Isrc -O2 -o $@ tests/gesture_test.c src/gesture.c src/ron.c src/sock.c

tests/macro_test: tests/macro_test.c src/macro.c src/macro.h src/gesture.h
	gcc -std=c11 -Wall -Wextra -Isrc -O2 -o $@ tests/macro_test.c src/macro.c

test: tests/gesture_test tests/macro_test
	./tests/gesture_test
	./tests/macro_test

clean:
	rm -f daeboard tests/gesture_test tests/macro_test
