#ifndef CZ_SORTING
#define CZ_SORTING

#include "mem.h"
#include <stddef.h>

/// Least to greatest randomized quicksort based on the Hoare pseudocode:
/// https://en.wikipedia.org/wiki/Quicksort
///
/// Structured to sort (int key, size_t value) pairs. Useful when sorting
/// integers but tracking their previous positions for usage elsewhere.
///
/// In local testing on 10_000_000 RANDOM integers, unrandomized quicksort was
/// about 4% faster than randomized. However, running the same test on
/// 10_000_000 SORTED integers was radically slower (2.702 seconds v. 10 minutes
/// before I killed the process).
/// So, this implementation accepts the minor rng cost on unsorted data as
/// defense against degenerating to O(n^2) on sorted data.
void quicksort(int *keys, Generic *vals, size_t len);

/// Randomized quicksort for u64 keys without values
void quicksort_u64(uint64_t *keys, size_t len);

/// Deduplicates sorted keys (and their associated values) so that
/// every key in the list is unique.
/// Treats values as uint. When duplicate keys appear, the key with
/// the lowest value is included in the output.
///
/// Returns the new length of the key array.
///
/// If the input isn't sorted, output is meaningless.
///
/// Ex: `dedup([
///   (1, 101),
///   (1, 100),
///   (2, 200),
///   (3, 300),
///   (3, 301),
///   (3, 302),
///   (4, 400),
///   (5, 500)
///   ]) == [
///   (1, 100),
///   (2, 200),
///   (3, 300),
///   (4, 400),
///   (5, 500)]`
// size_t dedup(int *sorted_keys, Generic *vals, size_t len);
size_t dedup_uint(int *sorted_keys, Generic *vals, size_t len);

/// Binary searches an integer array `sorted` of size `len` for `key`.
/// Returns the index of the first occurrence of key (if duplicates) or
/// the first element greater than the key.
size_t binary_search(const int *sorted, size_t len, int key);

#endif
