#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "hash_table.h"

// This code is designed to stress test your hash table implementation. You do
// not need to significantly change it, but you may want to vary the value of
// num_tests to control the amount of time and memory that benchmarking takes
// up. Compile and run it in the command line by typing:
// make benchmark; ./benchmark

int main(void) {

  HashTable *ht = NULL;
  const int NUM_TESTS = 50000000;
  int *keys = malloc(NUM_TESTS * sizeof(int));
  assert(keys != NULL);

  assert(allocate(&ht, NUM_TESTS) == 0);

  int seed = 2;
  srand(seed);
  printf("Performing stress test. Inserting, getting, and erasing %d "
         "keys.\n",
         NUM_TESTS);

  struct timeval stop, start;
  gettimeofday(&start, NULL);

  for (int i = 0; i < NUM_TESTS; i += 1) {
    int key = rand();
    int val = rand();
    keys[i] = key;
    assert(put(ht, key, val) == 0);
  }

  printf("Finished 'put', starting 'get'\n");
  const int NUM_VALS = 10;
  ValType vals[NUM_VALS];
  for (int i = 0; i < NUM_TESTS; i += 1) {
    int num_results = 0;
    get(ht, keys[i], vals, NUM_VALS, &num_results);
  }

  printf("Finished 'get', starting 'erase'\n");
  for (int i = 0; i < NUM_TESTS; i += 1) {
    erase(ht, keys[i]);
  }

  gettimeofday(&stop, NULL);
  double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 +
                (double)(stop.tv_sec - start.tv_sec);
  printf("Took %f seconds\n", secs);

  assert(deallocate(ht) == 0);
  free(keys);

  return 0;
}
