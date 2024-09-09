#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash_table.h"

void test_hash() {
  uint64_t total = 1000;
  int diff = 0;
  for (uint64_t num = 0; num < total; num++) {
    uint64_t hashed = htbl_hash(num, 1);
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
      fprintf(stderr,
              "Hashed a number other than 0 or 1: %" PRIu64 " -> %" PRIu64 "\n",
              num, hashed);
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

void test_basic_map() {
  int size = 10;
  HashTable ht = htbl_new(size);

  int key = 5;

  htbl_put(&ht, key, -19);
  htbl_put(&ht, key, -20);
  assert(htbl_size(&ht) == 2);

  size_t num_values = 1;
  ValType *values = malloc(num_values * sizeof(ValType));

  size_t num_results = htbl_get(&ht, key, values, num_values);
  if (num_results > num_values) {
    num_values = num_results;
    values = realloc(values, num_values * sizeof(ValType));
    num_results = htbl_get(&ht, key, values, num_values);
  }

  for (size_t i = 0; i < num_results; i++) {
    printf("(%d, %d) \n", key, values[i]);
  }
  free(values);

  htbl_erase(&ht, key);
  assert(htbl_size(&ht) == 0);

  htbl_deallocate(&ht);
}

// This is where you can implement your own tests for the hash table
// implementation.
int main(void) {
  test_hash();
  test_basic_map();

  return 0;
}
