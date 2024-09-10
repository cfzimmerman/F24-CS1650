CC=gcc -std=c99
CFLAGS = -ggdb3 -W -Wall -Wextra -O3
LDFLAGS = -lm
LIBS =

# Add CFLAGS `-DNDEBUG` to disable assertions and go way, way faster.
# Do not expect anything test-only to work with assertions enabled.
# Everything meant for production use must work with assertions disabled.

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
