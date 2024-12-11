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
  const int VAL_CT = 50000000;

  int seed = 2;
  srand(seed);
  printf("Performing stress test. Inserting, getting, and erasing %d "
         "keys.\n",
         VAL_CT);

  int *keys = malloc(VAL_CT * sizeof(int));
  assert(keys != NULL);

  struct timeval stop, start;
  gettimeofday(&start, NULL);

  HashTable ht = htbl_new(VAL_CT);

  printf("starting put 1\n");
  for (int i = 0; i < VAL_CT; i += 1) {
    int key = rand();
    int val = rand();
    keys[i] = key;
    htbl_put(&ht, key, val);
  }

  printf("starting get\n");
  const int NUM_VALS = 10;
  ValType vals[NUM_VALS];
  for (int i = 0; i < VAL_CT; i += 1) {
    htbl_get(&ht, keys[i], vals, NUM_VALS);
  }

  printf("starting erase 1\n");
  for (int i = 0; i < VAL_CT; i += 1) {
    htbl_erase(&ht, keys[i]);
  }

  assert(htbl_size(&ht) == 0);

  printf("starting put 2\n");
  for (int i = 0; i < VAL_CT; i += 1) {
    int key = rand();
    int val = rand();
    keys[i] = key;
    htbl_put(&ht, key, val);
  }

  printf("starting erase 2\n");
  for (int i = 0; i < VAL_CT; i += 1) {
    htbl_erase(&ht, keys[i]);
  }

  htbl_free(&ht);

  gettimeofday(&stop, NULL);
  double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 +
                (double)(stop.tv_sec - start.tv_sec);
  printf("Took %f seconds\n", secs);

  free(keys);

  return 0;
}
