.PHONY: all run clean

CC := clang

CFLAGS := -std=c23 -pedantic
CFLAGS += -Iinclude
CFLAGS += -lm -lglfw
CFLAGS += -O3
CFLAGS += -ffast-math
CFLAGS += -fopenmp
# CFLAGS += -g
# CFLAGS += -fsanitize=address

SHADERS := $(wildcard shader/*)
SOURCES := $(wildcard src/*)

all: verlet

verlet: $(SHADERS) $(SOURCES)
	 $(CC) $(CFLAGS) -o $@ $(SOURCES)

run: verlet
	./$<

clean:
	rm -f verlet
