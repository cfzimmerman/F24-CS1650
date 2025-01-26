#include "include/bitvec.h"
#include "include/mem.h"
#include <math.h>

BitVec bitvec_new(size_t capacity_bits) {
  size_t cap_bytes = ceil((double)capacity_bits / 8.);
  uint8_t *bits = NULL;
  if (cap_bytes > 0) {
    bits = MALLOC(sizeof(uint8_t) * cap_bytes);
  }
  return (BitVec){
      .bits = bits, .bit_cap = cap_bytes * 8, .bit_len = 0, .bit_ones = 0};
}

void bitvec_free(BitVec *bv) {
  if (bv->bit_cap != 0) {
    free(bv->bits);
  }
}

void bitvec_reserve(BitVec *bv, size_t capacity_bits) {
  if (capacity_bits <= bv->bit_cap) {
    return;
  }
  size_t new_cap_bytes = ceil((double)capacity_bits / 8.);
  if (bv->bit_cap == 0) {
    bv->bits = MALLOC(new_cap_bytes);
  } else {
    bv->bits = REALLOC(bv->bits, new_cap_bytes);
  }
  bv->bit_cap = new_cap_bytes * 8;
}
