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
    uint64_t hashed = pr_hash(num, 1);
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
  size_t d1 = pr_decide_reserve(256);
  assert(d1 == 512);
  assert(pr_decide_reserve(10) == 32);
  assert(pr_decide_reserve(0) == 0);
}

void test_basic_map() {
  HashTable ht = htbl_new(10);
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

  htbl_free(&ht);
}

void test_kv() {
  typedef struct kv {
    KeyType key;
    ValType val;
  } Kv;
#define NUM_PAIRS 8
  Kv pairs[NUM_PAIRS] = {(Kv){.key = 0, .val = 0},
                         (Kv){.key = 1, .val = 22},
                         (Kv){.key = 99, .val = -32},
                         (Kv){.key = -64, .val = 4},
                         (Kv){.key = INT32_MIN, .val = INT32_MIN},
                         (Kv){.key = INT32_MIN, .val = INT32_MAX},
                         (Kv){.key = INT32_MAX, .val = INT32_MIN},
                         (Kv){.key = INT32_MAX, .val = INT32_MAX}};
  for (size_t idx = 0; idx < NUM_PAIRS; idx++) {
    int key = pairs[idx].key;
    int val = pairs[idx].val;
    uint64_t kv = pr_make_key_val(key, val);
    assert(pr_parse_key(kv) == key);
    assert(pr_parse_val(kv) == val);
  }
}

int main(void) {
  test_kv();
  test_hash();
  test_reserve_for_capacity();
  test_basic_map();

  return 0;
}
