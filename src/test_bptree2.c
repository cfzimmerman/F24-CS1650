#include "include/bptree2.h"
#include "include/mem.h"
#include "include/sorting.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

// void print_node(void *node) {
//   if (((BPNodeHeader *)node)->is_leaf) {
//     BPNodeLeaf *leaf = node;
//     printf("leaf: [");
//     for (size_t key_idx = 0; key_idx < leaf->hd.key_ct; key_idx++) {
//       printf("(%d: {", leaf->hd.keys[key_idx]);
//       Vec *curr_vec = &leaf->vals[key_idx];
//       for (size_t vec_idx = 0; vec_idx < curr_vec->len; vec_idx++) {
//         printf("%lu,", (size_t)curr_vec->arr[vec_idx].uint);
//       }
//       printf("}), ");
//     }
//     printf("]\n");
//   } else {
//     BPNodeInternal *internal = node;
//     printf("internal: [");
//     for (size_t key_idx = 0; key_idx < internal->hd.key_ct; key_idx++) {
//       printf("%d,", internal->hd.keys[key_idx]);
//     }
//     printf("]\n");
//   }
// }

void test_small() {
  BPtree2 tree = bptree2_new();

  int nums[] = {28, 9, 31, 0, 1, 10, 30, 18, 8, 3, 36, 19, 6, 13, 7};
  int vals[] = {1014, 1036, 1035, 1046, 1043, 1006, 1016, 1031,
                1043, 1044, 1042, 1011, 1017, 1008, 1016};
  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    bptree2_insert(&tree, nums[idx], (Generic){.uint = vals[idx]});
  }

  for (size_t idx = 0; idx < sizeof(nums) / sizeof(int); idx++) {
    BP2Result got = bptree2_get(&tree, nums[idx]);
    assert(got.val);
    assert(got.key == nums[idx]);
    assert(got.val->len == 1);
    assert((int)got.val->arr[0].uint == vals[idx]);
  }

  bptree2_insert(&tree, 28, (Generic){.uint = 1013});
  bptree2_insert(&tree, 28, (Generic){.uint = 1012});
  bptree2_insert(&tree, 28, (Generic){.uint = 1011});
  bptree2_insert(&tree, 28, (Generic){.uint = 1010});

  BP2Result key28 = bptree2_get(&tree, 28);
  assert(key28.val);
  assert(key28.val->len == 5);
  bool expect[] = {false, false, false, false, false};
  for (size_t idx = 0; idx < key28.val->len; idx++) {
    expect[key28.val->arr[idx].uint - 1010] = true;
  }
  for (size_t idx = 0; idx < sizeof(expect) / sizeof(bool); idx++) {
    assert(expect[idx]);
  }

  int not_found[] = {-10, -13, -11, -15, -14, -15, -12, -16, -18, -19};
  for (size_t idx = 0; idx < sizeof(not_found) / sizeof(int); idx++) {
    assert(bptree2_get(&tree, not_found[idx]).key > not_found[idx]);
  }

  bptree2_free(&tree);
}

void test_large() {
  srand(1);
  const size_t LEN = 1000;

  BPtree2 tree = bptree2_new();
  int *keys = MALLOC(sizeof(int) * LEN);

  for (size_t key_idx = 0; key_idx < LEN; key_idx++) {
    keys[key_idx] = rand() % (LEN / 2);
    bptree2_insert(&tree, keys[key_idx], (Generic){.uint = key_idx});
  }

  for (size_t key_idx = 0; key_idx < LEN; key_idx++) {
    BP2Result res = bptree2_get(&tree, keys[key_idx]);
    assert(res.val);
    assert(res.key == keys[key_idx]);
    bool found_original_idx = false;
    for (size_t val_idx = 0; val_idx < res.val->len; val_idx++) {
      if (res.val->arr[val_idx].uint == key_idx) {
        found_original_idx = true;
        break;
      }
    }
    assert(found_original_idx);
  }

  free(keys);

  bptree2_free(&tree);
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

  BPtree2 tree = bptree2_new_loaded(keys, vals, len);
  for (size_t idx = 0; idx < len; idx++) {
    BP2Result got = bptree2_get(&tree, keys[idx]);
    assert(got.val);
    assert(got.key == keys[idx]);
    assert(got.val->len == 1);
    assert(got.val->arr[0].uint == vals[idx].uint);
  }

  for (int ct = -10; ct < 0; ct++) {
    BP2Result got = bptree2_get(&tree, ct);
    assert(ct < got.key);
  }

  {
    BPLeafIter iter = bptree2_iter(&tree, 2);
    for (size_t idx = 0; idx < len; idx++) {
      BPLeafIterItem next = bptree2_iter_next(&iter);
      assert(next.is_some);
      assert(next.key == keys[idx]);
      assert(next.val.uint == vals[idx].uint);
    }
    assert(!bptree2_iter_next(&iter).is_some);
  }

  {
    BPLeafIter iter = bptree2_iter(&tree, -10);
    for (size_t idx = 0; idx < len; idx++) {
      BPLeafIterItem next = bptree2_iter_next(&iter);
      assert(next.is_some);
      assert(next.key == keys[idx]);
      assert(next.val.uint == vals[idx].uint);
    }
    assert(!bptree2_iter_next(&iter).is_some);
  }

  {
    BPLeafIter iter = bptree2_iter(&tree, 11);
    for (size_t idx = 6; idx < len; idx++) {
      BPLeafIterItem next = bptree2_iter_next(&iter);
      assert(next.is_some);
      assert(next.key == keys[idx]);
      assert(next.val.uint == vals[idx].uint);
    }
    assert(!bptree2_iter_next(&iter).is_some);
  }

  {
    BPLeafIter iter = bptree2_iter(&tree, INT_MAX);
    BPLeafIterItem next = bptree2_iter_next(&iter);
    assert(next.key == keys[len - 1]);
    assert(!bptree2_iter_next(&iter).is_some);
  }

  bptree2_free(&tree);
  free(vals);
}

void test_load_large() {
  const size_t LEN = 1000;
  int *keys = MALLOC(sizeof(int) * LEN);
  Generic *pos = MALLOC(sizeof(Generic) * LEN);

  for (size_t idx = 0; idx < LEN; idx++) {
    // All evens
    keys[idx] = ((rand() % LEN) / 2) * 2;
    pos[idx] = (Generic){.uint = idx};
  }
  quicksort(keys, pos, LEN);

  BPtree2 tree = bptree2_new_loaded(keys, pos, LEN);
  for (size_t idx = 0; idx < LEN; idx++) {
    BP2Result got = bptree2_get(&tree, keys[idx]);
    assert(got.key == keys[idx]);
    assert(got.val);
    bool found_val = false;
    for (size_t entry = 0; entry < got.val->len; entry++) {
      if (got.val->arr[entry].uint == pos[idx].uint) {
        found_val = true;
        break;
      }
    }
    assert(found_val);
  }

  for (size_t num = 0; num < LEN / 4; num++) {
    int missing_key = ((rand() % LEN) / 2) * 2 - 1;
    BPLeafIter iter = bptree2_iter(&tree, missing_key);
    BPLeafIterItem next = bptree2_iter_next(&iter);
    assert(next.is_some);
    assert(missing_key < next.key);
  }

  BPLeafIter iter = bptree2_iter(&tree, INT_MIN);
  for (size_t idx = 0; idx < LEN; idx++) {
    BPLeafIterItem next = bptree2_iter_next(&iter);
    assert(next.is_some);
    assert(next.key == keys[idx]);
  }

  bptree2_free(&tree);
  free(keys);
  free(pos);
}

int main() {
  test_small();
  test_large();
  test_load_small();
  test_load_large();

  printf("✅ %s\n", __FILE__);
  return 0;
}
