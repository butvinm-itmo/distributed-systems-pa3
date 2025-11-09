SRC = $(wildcard *.c *.h)

all: pa2
	
pa2: $(SRC)
	clang -std=c99 -Wall -pedantic -Llib64 -lruntime *.c -o pa2
