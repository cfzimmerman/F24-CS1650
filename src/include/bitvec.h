#ifndef CZ_BITVEC
#define CZ_BITVEC

#include <stddef.h>
#include <stdint.h>

typedef struct BitVec {
  uint8_t *bits;
  size_t bit_len;  // vector fullness in BITS.
  size_t bit_cap;  // vector capacity in BITS.
  size_t bit_ones; // the number of one bits in 0..bit_len
} BitVec;

/// Creates a new bitvec with the given bit capacity.
/// Does not allocate if capacity == 0.
BitVec bitvec_new(size_t capacity_bits);

/// Frees the memory held by a bitvec.
void bitvec_free(BitVec *bv);

/// Reallocates bitvec to be larger if it can't currently accomodate
/// the requested capacity in BITS. Requested capacity is
/// rounded up to the next largest byte unit.
void bitvec_reserve(BitVec *bv, size_t capacity_bits);

#endif
