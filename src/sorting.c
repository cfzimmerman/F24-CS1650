#include "include/sorting.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

static inline void swap_i32(int *arr, size_t idx1, size_t idx2) {
  int temp = arr[idx1];
  arr[idx1] = arr[idx2];
  arr[idx2] = temp;
}

static inline void swap_generic(Generic *arr, size_t idx1, size_t idx2) {
  Generic temp = arr[idx1];
  arr[idx1] = arr[idx2];
  arr[idx2] = temp;
}

static inline void swap_u64(uint64_t *arr, size_t idx1, size_t idx2) {
  uint64_t temp = arr[idx1];
  arr[idx1] = arr[idx2];
  arr[idx2] = temp;
}

/*********** QUICKSORT FOR (INT, GENERIC) PAIRS ***********/

size_t pr_partition(int *keys, Generic *vals, size_t left_bound,
                    size_t right_bound) {
  size_t pivot_idx =
      left_bound + ((size_t)rand() % (right_bound - left_bound + 1));
  swap_i32(keys, pivot_idx, left_bound);
  swap_generic(vals, pivot_idx, left_bound);
  int pivot = keys[left_bound];

  ssize_t left = (ssize_t)left_bound - 1;
  size_t right = right_bound + 1;

  while (true) {
    while (keys[++left] < pivot) {
    }
    while (keys[--right] > pivot) {
    }
    if ((size_t)left >= right) {
      return right;
    }
    swap_i32(keys, left, right);
    swap_generic(vals, left, right);
  }
}

void pr_quicksort(int *keys, Generic *vals, ssize_t left, ssize_t right) {
  if (left < 0 || right < 0 || right <= left) {
    return;
  }
  size_t split = pr_partition(keys, vals, left, right);
  pr_quicksort(keys, vals, left, split);
  pr_quicksort(keys, vals, split + 1, right);
}

void quicksort(int *keys, Generic *vals, size_t len) {
  pr_quicksort(keys, vals, 0, len - 1);
}

/*********** QUICKSORT FOR (uint64_t) KEYS ***********/

size_t pr_partition_u64(uint64_t *keys, size_t left_bound, size_t right_bound) {
  size_t pivot_idx =
      left_bound + ((size_t)rand() % (right_bound - left_bound + 1));
  swap_u64(keys, pivot_idx, left_bound);
  uint64_t pivot = keys[left_bound];

  ssize_t left = (ssize_t)left_bound - 1;
  size_t right = right_bound + 1;

  while (true) {
    while (keys[++left] < pivot) {
    }
    while (keys[--right] > pivot) {
    }
    if ((size_t)left >= right) {
      return right;
    }
    swap_u64(keys, left, right);
  }
}

void pr_quicksort_u64(uint64_t *keys, ssize_t left, ssize_t right) {
  if (left < 0 || right < 0 || right <= left) {
    return;
  }
  size_t split = pr_partition_u64(keys, left, right);
  pr_quicksort_u64(keys, left, split);
  pr_quicksort_u64(keys, split + 1, right);
}

void quicksort_u64(uint64_t *keys, size_t len) {
  pr_quicksort_u64(keys, 0, len - 1);
}

// size_t dedup(int *sorted_keys, Generic *vals, size_t len) {
//   size_t slow = 0;
//   for (size_t fast = 1; fast < len; fast++) {
//     if (sorted_keys[slow] != sorted_keys[fast]) {
//       slow++;
//       sorted_keys[slow] = sorted_keys[fast];
//       vals[slow] = vals[fast];
//     }
//   }
//   return slow + 1;
// }

size_t dedup_uint(int *sorted_keys, Generic *vals, size_t len) {
  size_t slow = 0;
  for (size_t fast = 1; fast < len; fast++) {
    if (sorted_keys[slow] == sorted_keys[fast]) {
      if (vals[slow].uint > vals[fast].uint) {
        sorted_keys[slow] = sorted_keys[fast];
        vals[slow] = vals[fast];
      }
    } else {
      slow++;
      sorted_keys[slow] = sorted_keys[fast];
      vals[slow] = vals[fast];
    }
  }
  return slow + 1;
}

size_t binary_search(const int *sorted, size_t len, int key) {
  ssize_t left = 0;
  ssize_t right = len - 1;

  while (left < right) {
    ssize_t mid = (left + right) / 2;
    if (sorted[mid] < key) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  return left;
}
