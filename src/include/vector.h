#ifndef CZ_VECTOR
#define CZ_VECTOR

#include "mem.h"
#include "utils.h"

/// A growable array type.
typedef struct vector {
  size_t capacity;
  size_t len;
  Generic *arr;
} Vec;

/// Allocates a new vector with at least the requested capacity.
///
/// UNLESS the requested capacity is zero. If it's zero, the vector
/// doesn't do any malloc until pushed (same as Rust Vec::new).
///
/// PANICS if malloc fails.
Vec vec_new(size_t capacity);

/// Frees the vector's memory.
/// Be careful, this won't free the memory held by pointers in the vector.
/// If you're storing heap memory in this vector, make sure the vec is
/// empty before freeing it.
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
Generic vec_index(const Vec *vec, size_t idx);

/// Swaps the values at two indices.
///
/// PANICS if either index is out of bounds.
void vec_swap(Vec *vec, size_t idx1, size_t idx2);

/// Prints out the values in vec as uints.
void vec_print_uint(Vec *vec);

/// Reverses the elements in a vector.
void vec_reverse(Vec *vec);

/// Makes a vector empty again. Only call this on uint variants, it doesn't drop
/// anything on the heap.
void vec_clear(Vec *vec);

#endif
