#include "include/hash_table.h"
#include "include/mem.h"
#include "include/vector.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Allocates a new ListNode on the heap and returns a pointer
/// to it.
ListNode *lnode_new(int key, Generic val, ListNode *next) {
  ListNode *node = MALLOC(sizeof(ListNode));
  node->key = key;
  node->val = val;
  node->next = next;
  return node;
}

/// Given a ListNode, frees that node and all nodes that come after it.
void lnode_free_entire(ListNode *list) {
  while (list != NULL) {
    ListNode *temp = list;
    list = list->next;
    free(temp);
  }
}

/// Returns the bounds-checked bucket index a given key hashes to.
inline uint64_t pr_get_bucket_idx(HashTable *ht, int key) {
  uint64_t idx = htbl_hash((uint64_t)key, ht->arr_len_pow2);
  assert(idx < ht->arr_len);
  return idx;
}

/// Initializes a list node from the memory pool or mallocs one if all nodes
/// are currently in use.
inline ListNode *pr_take_lnode(HashTable *ht, int key, Generic val,
                               ListNode *next) {
  ListNode *node = ht->mem_pool;
  if (node == NULL) {
    return lnode_new(key, val, next);
  }
  ht->mem_pool = node->next;
  node->key = key;
  node->val = val;
  node->next = next;
  return node;
}

/// Returns a list node to the hash table's memory pool.
inline void pr_return_lnode(HashTable *ht, ListNode *node) {
  assert(node != NULL);
  node->next = ht->mem_pool;
  ht->mem_pool = node;
}

/// Suggests a size for the hash table based on how many elements it's expected
/// to hold.
inline uint64_t htbl_decide_reserve(size_t with_capacity) {
  /// Realloc the table if more than `1/OVERSIZE_FACTOR` buckets
  /// in the table are filled.
  const double OVERSIZE_FACTOR = 1.5;

  return pow(2, ceil(log2(with_capacity * OVERSIZE_FACTOR)));
}

HashTable htbl_new(size_t with_capacity) {
  uint64_t size = htbl_decide_reserve(with_capacity);

  size_t bucket_list_bytes = sizeof(ListNode *) * size;
  ListNode **buckets = MALLOC(bucket_list_bytes);
  memset((void *)buckets, 0, bucket_list_bytes);

  ListNode *mem_pool = NULL;
  // for (size_t i = 0; i < with_capacity; i++) {
  //   mem_pool = lnode_new(0, (Generic){.uint = 0}, mem_pool);
  // }

  return (HashTable){.mem_pool = mem_pool,
                     .arr = buckets,
                     .len = 0,
                     .arr_len = size,
                     .arr_len_pow2 = (uint64_t)floor(log2(size))};
}

void htbl_put(HashTable *ht, int key, Generic value) {
  size_t idx = pr_get_bucket_idx(ht, key);
  ListNode *curr_head = ht->arr[idx];

  ListNode *entry = pr_take_lnode(ht, key, value, curr_head);
  ht->arr[idx] = entry;
  ht->len++;
}

void htbl_get(HashTable *ht, int key, Vec *results) {
  assert(results->len == 0);
  size_t idx = pr_get_bucket_idx(ht, key);
  ListNode *head = ht->arr[idx];

  while (head != NULL) {
    ListNode *prev = head;
    head = head->next;
    if (prev->key == key) {
      vec_push(results, prev->val);
    }
  }
}

void htbl_erase(HashTable *ht, int key) {
  size_t idx = pr_get_bucket_idx(ht, key);
  ListNode *prev = NULL;
  ListNode *cursor = ht->arr[idx];

  while (cursor != NULL) {
    if (cursor->key != key) {
      prev = cursor;
      cursor = cursor->next;
      continue;
    }
    if (cursor == ht->arr[idx]) {
      ht->arr[idx] = cursor->next;
    }
    if (prev != NULL) {
      prev->next = cursor->next;
    }
    ListNode *temp = cursor;
    cursor = cursor->next;
    pr_return_lnode(ht, temp);
    ht->len--;
  }
}

// This method frees all memory occupied by the hash table.
// It returns an error code, 0 for success and -1 otherwise.
void htbl_free(HashTable *ht) {
  for (size_t idx = 0; idx < ht->arr_len; idx++) {
    ListNode *bucket = ht->arr[idx];
    if (bucket) {
      lnode_free_entire(bucket);
    }
  }
  free(ht->arr);
  if (ht->mem_pool != NULL) {
    lnode_free_entire(ht->mem_pool);
  }
}

/// From the Fibonacci hashing segment:
/// https://en.wikipedia.org/wiki/Hash_function
const uint64_t MULTIPLIER = 11400714819323198485ULL;
const uint64_t WORD_SIZE = 64;

/// Hashes a 64 bit key into the range `0..(2^domain_pow2)`.
/// Assumes the host machine's word size is 64 bits.
///
/// Take this mod domain if a more constrained hash is needed.
inline uint64_t htbl_hash(uint64_t key, uint64_t domain_pow2) {
  return (MULTIPLIER * key) >> (WORD_SIZE - domain_pow2);
}
