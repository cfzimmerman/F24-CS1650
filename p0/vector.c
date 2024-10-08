#include "vector.h"
#include "assert.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"

Vec vec_new(size_t capacity) {
  if (capacity < 4) {
    capacity = 4;
  }
  capacity = (size_t)pow(2., ceil(log2((double)capacity)));
  Generic *arr = malloc(sizeof(Generic) * capacity);
  assert(arr != NULL);
  return (Vec){.capacity = capacity, .len = 0, .arr = arr};
}

inline void vec_free(Vec *vec) { free(vec->arr); }

inline void pr_vec_realloc(Vec *vec) {
  size_t new_capacity = vec->capacity * 2;
  assert(new_capacity > vec->capacity);
  Generic *res = realloc(vec->arr, new_capacity * sizeof(Generic));
  assert(res != NULL);
  vec->arr = res;
  vec->capacity = new_capacity;
}

void vec_push(Vec *vec, Generic el) {
  if (vec->len == vec->capacity) {
    pr_vec_realloc(vec);
  }
  assert(vec->len < vec->capacity);
  vec->arr[vec->len] = el;
  vec->len++;
}

inline Generic vec_pop(Vec *vec) {
  assert(vec->len != 0);
  Generic val = vec->arr[vec->len - 1];
  vec->len--;
  return val;
}

inline Generic *vec_index(Vec *vec, size_t idx) {
  assert(idx < vec->len);
  return &vec->arr[idx];
}

void vec_swap(Vec *vec, size_t idx1, size_t idx2) {
  assert(idx1 < vec->len && idx2 < vec->len);
  Generic temp = vec->arr[idx1];
  vec->arr[idx1] = vec->arr[idx2];
  vec->arr[idx2] = temp;
}
