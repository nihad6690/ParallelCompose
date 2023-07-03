CC = gcc
CFLAGS = -Wall -std=c99 -pedantic

main: main.c clean
	$(CC) $(CFLAGS) -o main main.c
clean:
	rm -f main