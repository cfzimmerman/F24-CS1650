#include "hash_table.h"
#include "chunk_list.h"
#include "generic.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Sticks two i32s into a u64 with the key in the upper half and
///
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

/// Returns the bounds-checked bucket index a given key hashes to.
inline uint64_t pr_get_bucket_idx(HashTable *ht, KeyType key) {
  uint64_t idx = htbl_hash((uint64_t)key, ht->arr_len_pow2);
  assert(idx < ht->arr_len);
  return idx;
}

/// Initializes a list node from the memory pool or mallocs one if all nodes
/// are currently in use.
inline ChunkListNode *pr_take_lnode(HashTable *ht, KeyType key, ValType val,
                                    ChunkListNode *next) {
  ChunkListNode *node = ht->mem_pool;
  Generic entry = (Generic){.unsig = pr_make_key_val(key, val)};
  if (node == NULL) {
    return chl_node_new(entry, next);
  }
  ht->mem_pool = node->next;
  node->len = 1;
  node->arr[0] = entry;
  node->next = next;
  return node;
}

/// Returns a list node to the hash table's memory pool.
inline void pr_return_lnode(HashTable *ht, ChunkListNode *node) {
  assert(node != NULL);
  node->next = ht->mem_pool;
  ht->mem_pool = node;
}

HashTable htbl_new(size_t with_capacity) {
  double arr_len_pow2 = ceil(log2(with_capacity * 1.5));
  size_t size = pow(2., arr_len_pow2);

  size_t bucket_list_bytes = sizeof(ChunkListNode *) * size;
  ChunkListNode **buckets = malloc(bucket_list_bytes);
  assert(buckets != NULL);
  memset((void *)buckets, 0, bucket_list_bytes);

  ChunkListNode *mem_pool = NULL;
  for (size_t i = 0; i < with_capacity; i++) {
    ChunkListNode *new_head = chl_node_new((Generic){.unsig = 0}, mem_pool);
    assert(new_head != NULL);
    mem_pool = new_head;
  }

  return (HashTable){.mem_pool = mem_pool,
                     .arr = buckets,
                     .el_ct = 0,
                     .arr_len = size,
                     .arr_len_pow2 = (uint64_t)arr_len_pow2};
}

// This method inserts a key-value pair into the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if malloc is
// called and fails).
void htbl_put(HashTable *ht, KeyType key, ValType value) {
  assert(ht != NULL);
  size_t idx = pr_get_bucket_idx(ht, key);
  ChunkListNode *node = ht->arr[idx];

  ht->el_ct++;
  Generic entry = (Generic){.unsig = pr_make_key_val(key, value)};
  while (node != NULL) {
    if (node->len < CHL_ARR_SIZE) {
      node->arr[node->len++] = entry;
      return;
    }
    node = node->next;
  }

  ChunkListNode *new_node = pr_take_lnode(ht, key, value, ht->arr[idx]);
  ht->arr[idx] = new_node;
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
size_t htbl_get(HashTable *ht, KeyType key, ValType *values,
                size_t num_values) {
  assert(ht != NULL);
  assert(values != NULL);

  ChunkListIter iter = chl_iter_new(ht->arr[pr_get_bucket_idx(ht, key)]);
  size_t num_results = 0;
  Generic entry;
  while (chl_iter_next(&iter, &entry)) {
    if (pr_parse_key(entry.unsig) != key) {
      continue;
    }
    if (num_results < num_values) {
      values[num_results] = pr_parse_val(entry.unsig);
    }
    num_results++;
  }

  return num_results;
}

// This method erases all key-value pairs with a given key from the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if the
// hashtable is not allocated).
void htbl_erase(HashTable *ht, KeyType key) {
  assert(ht != NULL);
  size_t idx = pr_get_bucket_idx(ht, key);

  ChunkListNode *prev = NULL;
  ChunkListNode *curr = ht->arr[idx];

  while (curr != NULL) {
    size_t arr_idx = 0;
    while (arr_idx < curr->len) {
      if (pr_parse_key(curr->arr[arr_idx].unsig) != key) {
        arr_idx++;
      } else {
        curr->arr[arr_idx] = curr->arr[--curr->len];
        ht->el_ct--;
      }
    }

    if (curr->len != 0) {
      prev = curr;
      curr = curr->next;
      continue;
    }

    if (prev != NULL) {
      prev->next = curr->next;
    } else {
      ht->arr[idx] = curr->next;
    }
    ChunkListNode *temp = curr;
    curr = curr->next;
    pr_return_lnode(ht, temp);
  }
}

size_t htbl_size(HashTable *ht) {
  assert(ht != NULL);
  return ht->el_ct;
}

// This method frees all memory occupied by the hash table.
// It returns an error code, 0 for success and -1 otherwise.
void htbl_free(HashTable *ht) {
  assert(ht != NULL);
  for (size_t idx = 0; idx < ht->arr_len; idx++) {
    ChunkListNode *bucket = ht->arr[idx];
    if (bucket != NULL) {
      chl_node_free_all(bucket);
    }
  }
  free(ht->arr);
  if (ht->mem_pool != NULL) {
    chl_node_free_all(ht->mem_pool);
  }
}

/// From the Fibonacci hashing segment:
/// https://en.wikipedia.org/wiki/Hash_function
const uint64_t MULTIPLIER = 11400714819323198485ULL;
const uint64_t WORD_SIZE = 64;

/// Hashes a 64 bit key into the range `0..(2^domain_pow2)`.
/// Assumes the host machine's word size is 64 bits.
inline uint64_t htbl_hash(uint64_t key, uint64_t domain_pow2) {
  return (MULTIPLIER * key) >> (WORD_SIZE - domain_pow2);
}
