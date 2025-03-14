.PHONY: all run clean

CC := clang

CFLAGS := -std=c23 -pedantic
CFLAGS += -Iinclude
CFLAGS += -lglfw
CFLAGS += -O3
# CFLAGS += -g

SOURCES := src/main.c src/glad.c

all: verlet

verlet: shader/verlet.vert shader/verlet.frag $(SOURCES)
	 $(CC) $(CFLAGS) -o $@ $(SOURCES)

run: verlet
	./$<

clean:
	rm -f verlet
