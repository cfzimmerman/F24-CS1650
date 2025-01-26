#ifndef CZ_HASH_TABLE
#define CZ_HASH_TABLE

#include "mem.h"
#include "vector.h"

/// Notes:
/// This was my project zero, so even with updates now it still looks
/// a bit different from the rest.
///
/// Also, I tried three variations: this, one with chunk buckets, and another
/// with vector buckets. This beat the other two handily on a 50 million-element
/// test of create, insert, get, erase, free.

typedef struct ListNode {
  int key;
  Generic val;
  struct ListNode *next;
} ListNode;

typedef struct {
  /// How many buckets arr is allocated to handle
  size_t arr_len;

  /// An array of ListNode buckets of size arr_len. Buckets are NULL if
  /// there's currently no list at that location.
  ListNode **arr;

  /// The number of actual elements in the hash map
  size_t len;

  /// log2(arr_len). Cached to avoid computing it every time
  /// we hash a key.
  uint64_t arr_len_pow2;

  /// Unused ListNodes that can be reused to avoid unnecessary bucket
  /// allocations
  ListNode *mem_pool;
} HashTable;

HashTable htbl_new(size_t with_capacity);
void htbl_put(HashTable *ht, int key, Generic value);
void htbl_get(HashTable *ht, int key, Vec *results);
void htbl_erase(HashTable *ht, int key);
void htbl_free(HashTable *ht);

uint64_t htbl_hash(uint64_t key, uint64_t pow2);

#endif
