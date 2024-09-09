#ifndef CS165_HASH_TABLE // This is a header guard. It prevents the header from
                         // being included more than once.
#define CS165_HASH_TABLE

#include "stddef.h"
#include "stdint.h"
#include "vector.h"

// These are for readability, but the hash table absolutely expects
// these to be signed 32 bit integers.
typedef int32_t KeyType;
typedef int32_t ValType;

typedef struct hashtable {

  /// The number of actual elements in the hash map
  size_t el_ct;

  /// An array of Vector buckets.
  Vec *buckets;

  /// How many vectors in the buckets array.
  size_t num_buckets;

  /// log2(arr_len). Cached to avoid computing it every time
  /// we hash a key.
  uint64_t num_buckets_log2;
} HashTable;

HashTable htbl_new(size_t with_capacity);
void htbl_put(HashTable *ht, KeyType key, ValType value);
size_t htbl_get(HashTable *ht, KeyType key, ValType *values, size_t num_values);
void htbl_erase(HashTable *ht, KeyType key);
void htbl_free(HashTable *ht);
size_t htbl_size(HashTable *ht);

uint64_t pr_hash(uint64_t key, uint64_t domain);

uint64_t pr_make_key_val(int32_t key, int32_t val);
int32_t pr_parse_key(uint64_t kv);
int32_t pr_parse_val(uint64_t kv);

#endif
