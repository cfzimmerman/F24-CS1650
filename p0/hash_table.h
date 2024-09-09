#ifndef CS165_HASH_TABLE // This is a header guard. It prevents the header from
                         // being included more than once.
#define CS165_HASH_TABLE

#include "stddef.h"
#include "stdint.h"

typedef int32_t KeyType;
typedef int32_t ValType;

typedef struct list_node {
  KeyType key;
  ValType val;
  struct list_node *next;
} ListNode;

typedef struct hashtable {
  /// How many buckets arr is allocated to handle
  size_t arr_len;

  /// An array of ListNode buckets of size arr_len. Buckets are NULL if
  /// there's currently no list at that location.
  ListNode **arr;

  /// The number of actual elements in the hash map
  size_t el_ct;

  /// log2(arr_len). Cached to avoid computing it every time
  /// we hash a key.
  uint64_t arr_len_pow2;

  /// Unused ListNodes that can be reused to avoid unnecessary bucket
  /// allocations
  ListNode *mem_pool;
} HashTable;

HashTable htbl_new(size_t with_capacity);
void htbl_put(HashTable *ht, KeyType key, ValType value);
size_t htbl_get(HashTable *ht, KeyType key, ValType *values, size_t num_values);
void htbl_erase(HashTable *ht, KeyType key);
void htbl_free(HashTable *ht);
size_t htbl_size(HashTable *ht);

uint64_t htbl_hash(uint64_t key, uint64_t domain);

#endif
