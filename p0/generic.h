#ifndef CS165_GENERIC
#define CS165_GENERIC

#include <stdint.h>

/// A mega-offbrand attempt at some C generic. The user is wholly
/// responsible for tracking which variant they're using.
typedef union generic {
  void *ptr;
  uint64_t unsig;
  int64_t sig;
} Generic;

#endif
