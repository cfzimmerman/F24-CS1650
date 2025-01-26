#include "./include/vector.h"
#include "./include/utils.h"
#include "include/mem.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline Vec vec_new(size_t capacity) {
  if (capacity == 0) {
    return (Vec){.capacity = 0, .len = 0, .arr = NULL};
  }
  capacity = pow(2., ceil(log2(capacity)));
  Generic *arr = (Generic *)MALLOC(sizeof(Generic) * capacity);
  return (Vec){.capacity = capacity, .len = 0, .arr = arr};
}

inline void vec_free(Vec *vec) {
  if (vec->arr != NULL && vec->capacity != 0) {
    free(vec->arr);
  }
}

static inline void pr_vec_realloc(Vec *vec) {
  size_t new_capacity = vec->capacity * 2;
  if (new_capacity < 4) {
    new_capacity = 4;
  }
  // Same as malloc if capacity is zero and arr is still NULL.
  Generic *res = realloc(vec->arr, new_capacity * sizeof(Generic));
  if (res == NULL) {
    log_err("realloc failed at %s:%d\n", __FILE__, __LINE__);
    exit(1);
  }
  vec->arr = res;
  vec->capacity = new_capacity;
}

inline void vec_push(Vec *vec, Generic el) {
  if (__builtin_expect(vec->len == vec->capacity, false)) {
    pr_vec_realloc(vec);
  }
  assert(vec->len < vec->capacity);
  vec->arr[vec->len++] = el;
}

inline Generic vec_pop(Vec *vec) {
  assert(vec->len != 0);
  return vec->arr[--vec->len];
}

inline Generic vec_index(const Vec *vec, size_t idx) {
  // assert(idx < vec->len);
  return vec->arr[idx];
}

inline void vec_swap(Vec *vec, size_t idx1, size_t idx2) {
  assert(idx1 < vec->len && idx2 < vec->len);
  Generic temp = vec->arr[idx1];
  vec->arr[idx1] = vec->arr[idx2];
  vec->arr[idx2] = temp;
}

void vec_print_uint(Vec *vec) {
  // Malloc enough to print a maximum sized digit for every
  // entry in the vec plus a null terminator at the end plus
  // spaces and commas.
  size_t chars_per_digit = ceil(log10(UINT64_MAX));
  size_t bytes_to_alloc =
      sizeof(char) * ((chars_per_digit * vec->len) + 4 + (2 * vec->len));
  printf("allocating %lu bytes\n", bytes_to_alloc);
  char *mem = MALLOC(bytes_to_alloc);
  char *cursor = mem;
  *cursor++ = '[';
  for (size_t idx = 0; idx < vec->len; idx++) {
    cursor += sprintf(cursor, "%" PRIu64, vec_index(vec, idx).uint);
    if (idx + 1 < vec->len) {
      cursor += sprintf(cursor, ", ");
    }
  }
  *cursor++ = ']';
  *cursor++ = '\0';
  printf("%s\n", mem);
  free(mem);
}

void vec_reverse(Vec *vec) {
  if (vec->len < 2) {
    return;
  }
  size_t left = 0;
  size_t right = vec->len - 1;

  while (left < right) {
    vec_swap(vec, left, right);
    left++;
    right--;
  }
}

void vec_clear(Vec *vec) { vec->len = 0; }
