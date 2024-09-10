CC=gcc -std=c99
CFLAGS = -ggdb3 -W -Wall -Wextra -O3
LDFLAGS = -lm
LIBS =

default: main test benchmark test_vec

%.o: %.c %.h
	$(CC) -c -o $@ $< $(CFLAGS)

main: vector.o hash_table.o main.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

benchmark: vector.o hash_table.o benchmark.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test: vector.o hash_table.o test.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

test_vec: vector.o test_vec.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f main test benchmark test_vec *.o
