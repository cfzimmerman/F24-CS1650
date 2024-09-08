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

void test_reserve_for_capacity() {
  assert(htbl_decide_reserve(256) == 512);
  assert(htbl_decide_reserve(10) == 32);
  assert(htbl_decide_reserve(0) == 0);
}

void test_basic_map() {
  HashTable *ht = NULL;
  int size = 10;
  allocate(&ht, size);

  int key = 5;

  put(ht, key, -1);
  put(ht, key, -2);
  assert(htbl_size(ht) == 2);

  int num_values = 1;
  ValType *values = malloc(num_values * sizeof(ValType));
  int num_results = 0;

  get(ht, key, values, num_values, &num_results);
  if (num_results > num_values) {
    num_values = num_results;
    values = realloc(values, num_values * sizeof(ValType));
    get(ht, key, values, num_values, &num_results);
  }

  for (int i = 0; i < num_results; i++) {
    printf("%d: (%d, %d) \n", i, key, values[i]);
  }
  free(values);

  erase(ht, key);
  assert(htbl_size(ht) == 0);

  deallocate(ht);
}

// This is where you can implement your own tests for the hash table
// implementation.
int main(void) {
  test_hash();
  test_reserve_for_capacity();
  test_basic_map();

  return 0;
}
