#include "include/bptree.h"
#include "include/sorting.h"
#include "include/utils.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

void test_small() {
  BPtree tree = bptree_new();

  int nums[] = {28, 9, 31, 0, 1, 10, 30, 18, 8, 3, 36, 19, 6, 13, 7};
  int vals[] = {1014, 1036, 1035, 1046, 1043, 1006, 1016, 1031,
                1043, 1044, 1042, 1011, 1017, 1008, 1016};
  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    bptree_insert(&tree, nums[idx], (Generic){.uint = vals[idx]});
  }

  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    BpResult got = bptree_get(&tree, nums[idx]);
    assert(got.key == nums[idx]);
    assert((int)got.val.uint == vals[idx]);
  }

  bptree_free(&tree);
}

void test_get_small() {
  BPtree tree = bptree_new();

  int keys[] = {3, -10, 6, 0, -1, -6, -1, 1, -2, -7};
  uint64_t vals[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  size_t len = sizeof(keys) / sizeof(int);
  quicksort(keys, (Generic *)vals, len);
  int max_key = INT_MIN;
  for (size_t idx = 0; idx < len; idx++) {
    bptree_insert(&tree, keys[idx], (Generic){.uint = vals[idx]});
    max_key = max_sig(max_key, keys[idx]);
  }

  // printf("kvs: [");
  // for (size_t idx = 0; idx < len; idx++) {
  //   printf("(%d, %lu), ", keys[idx], (size_t)vals[idx]);
  // }
  // printf("]\n");

  for (int key = -15; key < 15; key++) {
    BpResult got = bptree_get(&tree, key);
    if (got.key < key) {
      assert(key > max_key);
      continue;
    }
    assert(key <= got.key);
    assert(got.key == keys[binary_search(keys, len, key)]);
  }

  bptree_free(&tree);
}

void test_get_large() {
  const size_t LEN = 1000;
  BPtree tree = bptree_new();

  int *keys = MALLOC(sizeof(int) * LEN);
  Generic *vals = MALLOC(sizeof(Generic) * LEN);

  for (size_t idx = 0; idx < LEN; idx++) {
    keys[idx] = (rand() % LEN) - (LEN / 2);
    vals[idx].uint = idx;
  }

  quicksort(keys, vals, LEN);
  size_t len = dedup_uint(keys, vals, LEN);
  for (size_t idx = 0; idx < len; idx++) {
    bptree_insert(&tree, keys[idx], vals[idx]);
  }

  for (int key = -(int)LEN; key < (int)LEN; key++) {
    BpResult bp = bptree_get(&tree, key);
    size_t bsearch_idx = binary_search(keys, len, key);
    assert(keys[bsearch_idx] == bp.key);
    assert(vals[bsearch_idx].uint == bp.val.uint);
  }

  free(keys);
  free(vals);
  bptree_free(&tree);
}

size_t count_keys(BPtreeNode *node, int key) {
  size_t count = 0;
  if (node->is_leaf) {
    for (size_t idx = 0; idx < node->key_ct; idx++) {
      count += (node->keys[idx] == key);
    }
    return count;
  }
  for (size_t idx = 0; idx <= node->key_ct; idx++) {
    count += count_keys(node->meta[idx].ptr, key);
  }
  return count;
}

void test_large() {
  BPtree tree = bptree_new();
  for (size_t ct = 0; ct < 1000; ct++) {
    int key = rand();
    size_t val = rand();

    bptree_insert(&tree, key, (Generic){.uint = val});
    assert(count_keys(tree.root, key) == 1);

    BpResult res = bptree_get(&tree, key);
    assert(res.key == key);
    size_t found = res.val.uint;
    assert(found == val);
  }

  bptree_free(&tree);
}

void test_load_small() {
  int keys[] = {2, 4, 5, 8, 9, 10, 11, 13, 14, 15, 16, 20, 24, 27, 32, 33, 34};
  size_t vals_raw[] = {0, 1,  2,  3,  4,  5,  6,  7, 8,
                       9, 10, 11, 12, 13, 14, 15, 16};

  size_t len = sizeof(keys) / sizeof(int);
  assert(len == sizeof(vals_raw) / sizeof(size_t));

  Generic *vals = MALLOC(sizeof(Generic) * len);
  for (size_t idx = 0; idx < len; idx++) {
    vals[idx] = (Generic){.uint = vals_raw[idx]};
  }

  BPtree tree = bptree_new_loaded(keys, vals, len);
  for (size_t idx = 0; idx < len; idx++) {
    BpResult got = bptree_get(&tree, keys[idx]);
    assert(got.key == keys[idx]);
    assert(got.val.uint == vals[idx].uint);
  }

  for (int key = -10; key < 0; key++) {
    BpResult got = bptree_get(&tree, key);
    assert(got.key > key);
  }

  bptree_free(&tree);
  free(vals);
}

void test_load_large() {
  srand(32);
  const size_t LEN = 1000;
  int *keys = MALLOC(sizeof(int) * LEN);
  Generic *pos = MALLOC(sizeof(Generic) * LEN);

  for (size_t idx = 0; idx < LEN; idx++) {
    keys[idx] = rand();
    pos[idx] = (Generic){.uint = idx};
  }
  quicksort(keys, pos, LEN);
  size_t len = dedup_uint(keys, pos, LEN);

  BPtree tree = bptree_new_loaded(keys, pos, len);
  for (size_t idx = 0; idx < len; idx++) {
    BpResult res = bptree_get(&tree, keys[idx]);
    assert(res.key == keys[idx]);
    assert(res.val.uint == pos[idx].uint);
  }

  for (int missing_key = -100; missing_key < 0; missing_key++) {
    assert(missing_key < bptree_get(&tree, missing_key).key);
  }

  bptree_free(&tree);
  free(keys);
  free(pos);
}

int main() {
  test_small();
  test_get_small();
  test_get_large();
  test_large();
  test_load_small();
  test_load_large();

  printf("✅ %s\n", __FILE__);
  return 0;
}
