#include "hash_table.h"
#include "assert.h"
#include "limits.h"
#include "math.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "vector.h"

/// Returns the bounds-checked bucket index a given key hashes to.
/// If the bucket at this location hasn't been instantiated yet, a new
/// vector is created and placed there.
inline Vec *pr_get_bucket(HashTable *ht, KeyType key) {
  uint64_t idx = pr_hash((uint64_t)key, ht->num_buckets_log2);
  assert(idx < ht->num_buckets);
  return &ht->buckets[idx];
}

// Initialize the components of a hashtable.
// The size parameter is the expected number of elements to be inserted.
// This method returns an error code, 0 for success and -1 otherwise (e.g., if
// the parameter passed to the method is not null, if malloc fails, etc).
HashTable htbl_new(size_t with_capacity) {
  uint64_t size = pow(2., ceil(log2(with_capacity)));
  if (size < 8) {
    size = 8;
  }
  Vec *buckets = malloc(sizeof(Vec) * size);
  assert(buckets != NULL);

  for (size_t idx = 0; idx < size; idx++) {
    buckets[idx] = vec_new(4);
  }

  return (HashTable){.el_ct = 0,
                     .num_buckets = size,
                     .num_buckets_log2 = (uint64_t)floor(log2(size)),
                     .buckets = buckets};
}

void htbl_free(HashTable *ht) {
  for (size_t idx = 0; idx < ht->num_buckets; idx++) {
    vec_free(&ht->buckets[idx]);
  }
  free(ht->buckets);
}

// This method inserts a key-value pair into the hash table.
void htbl_put(HashTable *ht, KeyType key, ValType value) {
  // assert(ht != NULL);
  vec_push(pr_get_bucket(ht, key),
           (Generic){.unsig = pr_make_key_val(key, value)});
  ht->el_ct++;
}

// This method retrieves entries with a matching key and stores the
// corresponding values in the values array. The size of the values array is
// given by the parameter num_values. If there are more matching entries than
// num_values, they are not stored in the values array to avoid a buffer
// overflow. The function returns the number of matching entries using the
// num_results pointer. If the value of num_results is greater than num_values,
// the caller can invoke this function again (with a larger buffer) to get
// values that it missed during the first call. This method returns an error
// code, 0 for success and -1 otherwise (e.g., if the hashtable is not
// allocated).
inline size_t htbl_get(HashTable *ht, KeyType key, ValType *values,
                       size_t num_values) {
  // assert(ht != NULL);
  // assert(values != NULL);
  Vec *bucket = pr_get_bucket(ht, key);

  size_t num_results = 0;
  for (size_t idx = 0; idx < bucket->len; idx++) {
    uint64_t kv = bucket->arr[idx].unsig;
    if (pr_parse_key(kv) != key) {
      continue;
    }
    if (num_results < num_values) {
      values[num_results] = pr_parse_val(kv);
    }
    num_results++;
  }

  return num_results;
}

// This method erases all key-value pairs with a given key from the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if the
// hashtable is not allocated).
void htbl_erase(HashTable *ht, KeyType key) {
  // assert(ht != NULL);
  Vec *bucket = pr_get_bucket(ht, key);

  size_t idx = 0;
  while (idx < bucket->len) {
    uint64_t kv = vec_index(bucket, idx).unsig;
    if (pr_parse_key(kv) != key) {
      idx++;
      continue;
    }
    if (bucket->len > 1) {
      vec_swap(bucket, idx, bucket->len - 1);
    }
    vec_pop(bucket);
    ht->el_ct--;
  }
}

inline size_t htbl_size(HashTable *ht) {
  assert(ht != NULL);
  return ht->el_ct;
}

/// From the Fibonacci hashing segment:
/// https://en.wikipedia.org/wiki/Hash_function
const uint64_t MULTIPLIER = 11400714819323198485ULL;
const uint64_t WORD_SIZE = 64;

/// Hashes a 64 bit key into the range `0..(2^domain_pow2)`.
/// Assumes the host machine's word size is 64 bits.
inline uint64_t pr_hash(uint64_t key, uint64_t domain_pow2) {
  return (MULTIPLIER * key) >> (WORD_SIZE - domain_pow2);
}

/// Sticks two i32s into a u64 with the key in the upper half and
/// the value in the lower half.
/// The resulting number is meaningless and should only be used as
/// an input to one of the parse methods.
inline uint64_t pr_make_key_val(int32_t key, int32_t val) {
  return ((uint64_t)(uint32_t)key << 32) | (uint32_t)val;
}

/// Retrieves the key field from a u64 composed of (key i32, val i32).
inline int32_t pr_parse_key(uint64_t kv) {
  return (int32_t)(uint32_t)(kv >> 32);
}

/// Retrieves the val field from a u64 composed of (key i32, val i32).
inline int32_t pr_parse_val(uint64_t kv) {
  return (int32_t)(uint32_t)(kv & UINT32_MAX);
}
