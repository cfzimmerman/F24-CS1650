#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash_table.h"

void test_hash() {
  uint64_t total = 1000;
  int diff = 0;
  for (uint64_t num = 0; num < total; num++) {
    uint64_t hashed = hash(num, 1);
    switch (hashed) {
    case 0: {
      diff--;
      break;
    }
    case 1: {
      diff++;
      break;
    }
    default: {
      fprintf(stderr, "Hashed a number other than 0 or 1: %llu -> %llu\n", num,
              hashed);
      abort();
    }
    }
  }
  if ((uint64_t)abs(diff) > total / 4) {
    fprintf(stderr, "Hash skewed far in one direction: %i\n", diff);
    abort();
  }
  return;
}

// This is where you can implement your own tests for the hash table
// implementation.
int main(void) {

  hashtable *ht = NULL;
  int size = 10;
  allocate(&ht, size);

  int key = 0;
  int value = -1;

  put(ht, key, value);

  int num_values = 1;

  valType *values = malloc(1 * sizeof(valType));

  int *num_results = NULL;

  get(ht, key, values, num_values, num_results);
  if ((*num_results) > num_values) {
    values = realloc(values, (*num_results) * sizeof(valType));
    get(ht, 0, values, num_values, num_results);
  }

  for (int i = 0; i < (*num_results); i++) {
    printf("value of %d is %d \n", i, values[i]);
  }
  free(values);

  erase(ht, 0);

  deallocate(ht);

  test_hash();

  return 0;
}
