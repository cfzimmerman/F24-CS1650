#ifndef CS165_VECTOR
#define CS165_VECTOR

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

/// A mega-offbrand attempt at some C generic. The user is wholly
/// responsible for tracking which variant they're using.
typedef union generic {
  void *ptr;
  uint64_t unsig;
  int64_t sig;
} Generic;

/// A growable array type.
typedef struct vector {
  size_t capacity;
  size_t len;
  Generic *arr;
} Vec;

// TODO: make vec_new cap = 0 lazily allocate!

/// Allocates a new vector with at least the requested capacity.
///
/// UNLESS the requested capacity is zero. If it's zero, the vector
/// doesn't do any malloc (same as Rust Vec::new).
///
/// PANICS if malloc fails.
Vec vec_new(size_t capacity);

/// Frees the vector's memory.
/// Be careful, this won't free the memory held by pointers in the vector.
/// If you're storing heap memory in this vector, make sure the vec is
/// empty before freeing it.
///
/// PANICS if free fails.
void vec_free(Vec *vec);

/// Pushes a new element onto the vector. The caller is responsible for
/// consistently using the same element type.
///
/// PANICS if vector realloc fails.
void vec_push(Vec *vec, Generic el);

/// Pops the last element of the vector into el.
///
/// PANICS if the vector currently has length 0.
Generic vec_pop(Vec *vec);

/// Returns the element at idx.
///
/// PANICS if the index is out of bounds.
Generic vec_index(Vec *vec, size_t idx);

/// Swaps the values at two indices.
///
/// PANICS if either index is out of bounds.
void vec_swap(Vec *vec, size_t idx1, size_t idx2);

#endif
