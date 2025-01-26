#ifndef CZ_MEM
#define CZ_MEM

#include "utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/// Malloc wrapper that panics on failure.
static inline void *malloc_or_bust(size_t size, const char *file, int line) {
  void *ptr = malloc(size);
  if (ptr == NULL) {
    log_err("Malloc failed at %s:%d\n", file, line);
    exit(1);
  }
  return ptr;
}

#define MALLOC(size) malloc_or_bust(size, __FILE__, __LINE__)

/// Realloc wrapper that panics on failure.
static inline void *realloc_or_bust(void *ptr, size_t size, const char *file,
                                    int line) {
  void *new_ptr = realloc(ptr, size);
  if (new_ptr == NULL) {
    log_err("Realloc failed at %s:%d\n", file, line);
    exit(1);
  }
  return new_ptr;
}

#define REALLOC(ptr, size) realloc_or_bust(ptr, size, __FILE__, __LINE__)

/// Some C value. The user is wholly responsible for tracking
/// which variant they're using.
typedef union {
  void *ptr;
  uint64_t uint;
} Generic;

enum GenericType { NUM, PTR };

typedef struct {
  bool is_some;
  Generic val;
} OptGeneric;

static inline OptGeneric generic_some(Generic gen) {
  return (OptGeneric){.is_some = true, .val = gen};
}

static inline OptGeneric generic_none() {
  return (OptGeneric){.is_some = false};
}

#endif
