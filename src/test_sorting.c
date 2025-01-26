#include "include/sorting.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_quicksort_rng(size_t len) {
  assert(len > 1);

  int nums[len];
  int unsorted[len];
  Generic pos[len];
  for (size_t idx = 0; idx < len; idx++) {
    nums[idx] = rand();
    unsorted[idx] = nums[idx];
    pos[idx] = (Generic){.uint = idx};
  }

  // Implicitly tests performance on both random and presorted data.
  quicksort(nums, pos, len);
  quicksort(nums, pos, len);
  for (size_t idx = 1; idx < len; idx++) {
    assert(nums[idx - 1] <= nums[idx]);
    assert(nums[idx] == unsorted[pos[idx].uint]);
  }
}

void test_dedup_small() {
  int keys[] = {-5, -5, -2, 0, 0, 0, 1, 4, 7, 8, 8, 9, 9};
  size_t vals_raw[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  size_t len = sizeof(keys) / sizeof(int);
  assert(len == sizeof(vals_raw) / sizeof(size_t));
  Generic vals[len];
  for (size_t idx = 0; idx < len; idx++) {
    vals[idx] = (Generic){.uint = vals_raw[idx]};
  }

  size_t new_len = dedup_uint(keys, vals, len);
  assert(new_len == 8);

  int expected_keys[] = {-5, -2, 0, 1, 4, 7, 8, 9};
  size_t expected_vals[] = {0, 2, 3, 6, 7, 8, 9, 11};
  for (size_t idx = 0; idx < new_len; idx++) {
    assert(keys[idx] == expected_keys[idx]);
    assert((size_t)vals[idx].uint == expected_vals[idx]);
  }
}

void test_dedup_large() {
  const size_t LEN = 1000;

  int *keys = MALLOC(sizeof(int) * LEN);
  Generic *pos = MALLOC(sizeof(Generic) * LEN);

  for (size_t idx = 0; idx < LEN; idx++) {
    keys[idx] = rand() % LEN;
    pos[idx] = (Generic){.uint = idx};
  }

  int *keys_copy = MALLOC(sizeof(int) * LEN);
  memcpy(keys_copy, keys, sizeof(int) * LEN);

  quicksort(keys, pos, LEN);

  size_t new_len = dedup_uint(keys, pos, LEN);
  assert(new_len <= LEN);

  for (size_t idx = 0; idx < new_len; idx++) {
    size_t position = pos[idx].uint;
    if (0 < idx) {
      assert(keys[idx - 1] < keys[idx]);
    }

    // Verify no entries before `position` have this key
    for (size_t orig_idx = 0; orig_idx < position; orig_idx++) {
      assert(keys_copy[orig_idx] != keys[idx]);
    }
    assert(keys_copy[position] == keys[idx]);
  }

  free(keys);
  free(keys_copy);
  free(pos);
}

// Does the same thing as binary search, just less efficiently.
size_t scan_search(int *nums, size_t len, int key) {
  for (size_t idx = 0; idx < len; idx++) {
    if (key <= nums[idx]) {
      return idx;
    }
  }
  return len;
}

void test_binary_search(size_t test_ct, size_t nums_per_test) {
  int *buf = MALLOC(sizeof(int) * nums_per_test);
  Generic *unused = MALLOC(sizeof(Generic) * nums_per_test);
  memset(unused, 0, sizeof(Generic) * nums_per_test);
  for (size_t test_id = 0; test_id < test_ct; test_id++) {
    for (size_t num = 0; num < nums_per_test; num++) {
      buf[num] = rand() % nums_per_test;
    }

    quicksort(buf, unused, nums_per_test);
    int key = rand() % nums_per_test;

    size_t linear = scan_search(buf, nums_per_test, key);
    size_t binary = binary_search(buf, nums_per_test, key);
    assert(linear == binary);
  }
  free(buf);
  free(unused);
}

void test_quicksort_u64(size_t len) {
  uint64_t *nums = MALLOC(sizeof(uint64_t) * len);
  for (size_t idx = 0; idx < len; idx++) {
    nums[idx] = rand();
  }

  quicksort_u64(nums, len);

  for (size_t idx = 1; idx < len; idx++) {
    assert(nums[idx - 1] <= nums[idx]);
  }

  free(nums);
}

int main() {
  test_quicksort_rng(10);
  test_quicksort_rng(1000);
  test_quicksort_rng(100000);
  test_dedup_small();
  test_dedup_large();
  test_binary_search(100, 1000);
  test_quicksort_u64(100000);

  printf("✅ %s\n", __FILE__);
  return 0;
}
