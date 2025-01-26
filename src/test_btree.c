#include "include/btree.h"
#include "include/mem.h"
#include <assert.h>
#include <stdio.h>

size_t count_keys(BtreeNode *node, int key) {
  size_t count = 0;
  for (size_t idx = 0; idx < node->key_ct; idx++) {
    if (node->keys[idx] == key) {
      count++;
    }
  }
  if (!node->is_leaf) {
    for (size_t idx = 0; idx <= node->key_ct; idx++) {
      count += count_keys(node->children[idx], key);
    }
  }
  return count;
}

void test_small() {
  Btree tree = btree_new();

  int nums[] = {11, 17, 9, 21, 12, 19, 8};
  int vals[] = {1040, 1060, 1099, 1085, 1032, 1086, 1024};
  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    btree_insert(&tree, nums[idx], (Generic){.uint = vals[idx]});
  }

  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    OptGeneric got = btree_get(&tree, nums[idx]);
    assert(got.is_some);
    assert((int)got.val.uint == vals[idx]);
  }

  int not_found[] = {50, 20, 51, 120, 61, 175, 145, 124, 187, 165, 161};
  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    assert(!btree_get(&tree, not_found[idx]).is_some);
  }

  btree_free(&tree);
}

void test_large() {
  Btree tree = btree_new();
  for (size_t ct = 0; ct < 1000; ct++) {
    int key = rand();
    size_t val = rand();

    btree_insert(&tree, key, (Generic){.uint = val});
    assert(count_keys(tree.root, key) == 1);

    OptGeneric res = btree_get(&tree, key);
    assert(res.is_some);
    size_t found = res.val.uint;
    assert(found == val);
  }

  btree_free(&tree);
}

int main() {
  test_small();
  test_large();

  printf("✅ %s\n", __FILE__);
  return 0;
}
