#include "include/hash_table.h"
#include "include/vector.h"
#include <assert.h>

void test_basic() {
  const int VAL_CT = 20;
  HashTable ht = htbl_new(VAL_CT);

  int seed = 1;
  srand(seed);
  int keys[VAL_CT];
  Generic values[VAL_CT];

  for (int i = 0; i < VAL_CT; i += 1) {
    keys[i] = rand();
    values[i].uint = rand();
    htbl_put(&ht, keys[i], values[i]);
  }

  // Get
  Vec results = vec_new(1);
  for (int i = 0; i < VAL_CT; i += 1) {
    int index = rand() % VAL_CT;
    int target_key = keys[index];

    vec_clear(&results);
    htbl_get(&ht, target_key, &results);

    assert(results.len == 1);
    assert(results.arr[0].uint == values[index].uint);
  }

  vec_free(&results);

  // Erase
  vec_clear(&results);
  for (int i = 0; i < VAL_CT; i += 1) {
    int target_key = keys[i];
    htbl_erase(&ht, target_key);

    htbl_get(&ht, target_key, &results);
    assert(results.len == 0);
  }

  htbl_free(&ht);
}

void test_large(size_t val_ct) {
  int *keys = MALLOC(val_ct * sizeof(int));
  HashTable ht = htbl_new(val_ct);

  for (size_t idx = 0; idx < val_ct; idx += 1) {
    int key = rand();
    int val = rand();
    keys[idx] = key;
    htbl_put(&ht, key, (Generic){.uint = val});
  }

  {
    Vec res = vec_new(0);
    for (size_t idx = 0; idx < val_ct; idx += 1) {
      vec_clear(&res);
      htbl_get(&ht, keys[idx], &res);
      assert(res.len > 0);
    }
    vec_free(&res);
  }

  for (size_t idx = 0; idx < val_ct; idx += 1) {
    htbl_erase(&ht, keys[idx]);
  }

  assert(ht.len == 0);

  for (size_t idx = 0; idx < val_ct; idx += 1) {
    int key = rand();
    int val = rand();
    keys[idx] = key;
    htbl_put(&ht, key, (Generic){.uint = val});
  }

  for (size_t idx = 0; idx < val_ct; idx += 1) {
    htbl_erase(&ht, keys[idx]);
  }

  htbl_free(&ht);
  free(keys);
}

void test_hash(size_t sample_ct) {
  srand(17);
  uint64_t buckets[] = {0, 0, 0, 0, 0, 0, 0};
  size_t domain = sizeof(buckets) / sizeof(uint64_t);

  for (size_t ct = 0; ct < sample_ct; ct++) {
    size_t out = htbl_hash(rand(), 64) % domain;
    assert(out < domain);
    buckets[out]++;
  }

  for (size_t idx = 0; idx < domain; idx++) {
    // printf("(%lu): %lu\n", idx, (size_t)buckets[idx]);
    assert((double)buckets[idx] / (double)sample_ct < (2. / domain));
  }
}

int main(void) {
  test_basic();
  test_large(1000);
  test_hash(10000);

  printf("✅ %s\n", __FILE__);
  return 0;
}
