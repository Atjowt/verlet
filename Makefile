.PHONY: all run clean

CC := clang

CFLAGS := -std=c23 -pedantic
CFLAGS += -Iinclude
CFLAGS += -lm -lglfw
# CFLAGS += -g
# CFLAGS += -fsanitize=address
CFLAGS += -O3 -ffast-math
# CFLAGS += -fopenmp

SHADERS := $(wildcard shader/*)
SOURCES := $(wildcard src/*)

all: verlet

verlet: $(SHADERS) $(SOURCES)
	 $(CC) $(CFLAGS) -o $@ $(SOURCES)

run: verlet
	./$<

clean:
	rm -f verlet
