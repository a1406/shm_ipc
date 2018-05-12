all: test test2

test: test.c
	gcc -g -O0 -o test test.c

test2: test2.c
	gcc -g -O3 -o test2 test2.c
