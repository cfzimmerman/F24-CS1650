#include "hash_table.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Ditch the linked list and memory pool. Make a vector and run that.

/// Allocates a new ListNode on the heap and returns a pointer
/// to it. Returns NULL if malloc failed.
ListNode *lnode_new(KeyType key, ValType val, ListNode *next) {
  ListNode *node = malloc(sizeof(ListNode));
  if (node == NULL) {
    return NULL;
  }
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
inline uint64_t pr_get_bucket_idx(HashTable *ht, KeyType key) {
  uint64_t idx = htbl_hash((uint64_t)key, ht->arr_len_pow2);
  assert(idx < ht->arr_len);
  return idx;
}

/// Initializes a list node from the memory pool or mallocs one if all nodes
/// are currently in use.
inline ListNode *pr_take_lnode(HashTable *ht, KeyType key, ValType val,
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
  const double OVERSIZE_FACTOR = 2.;

  return pow(2, ceil(log2(with_capacity * OVERSIZE_FACTOR)));
}

// Initialize the components of a hashtable.
// The size parameter is the expected number of elements to be inserted.
// This method returns an error code, 0 for success and -1 otherwise (e.g., if
// the parameter passed to the method is not null, if malloc fails, etc).
int htbl_allocate(HashTable **ht, int with_capacity) {
  uint64_t size = htbl_decide_reserve(with_capacity);

  // Allocate the table itself
  HashTable *table = malloc(sizeof(HashTable));
  if (table == NULL) {
    return -1;
  }

  // Allocate the memory pool of list nodes equal to the expected number of
  // elements
  ListNode *mem_pool = NULL;
  for (int i = 0; i < with_capacity; i++) {
    ListNode *new_head = lnode_new(0, 0, mem_pool);
    if (new_head == NULL) {
      if (mem_pool != NULL) {
        lnode_free_entire(mem_pool);
      }
      return -1;
    }
    mem_pool = new_head;
  }

  // Allocate the hash table bucket list, set every bucket to NULL initially.
  size_t bucket_list_size = sizeof(ListNode *) * size;
  ListNode **buckets = malloc(bucket_list_size);
  if (buckets == NULL) {
    free(table);
    lnode_free_entire(mem_pool);
    return -1;
  }
  memset((void *)buckets, 0, bucket_list_size);

  table->arr = buckets;
  table->arr_len = size;
  table->el_ct = 0;
  table->arr_len_pow2 = (uint64_t)floor(log2(size));
  table->mem_pool = mem_pool;

  *ht = table;

  return 0;
}

// This method inserts a key-value pair into the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if malloc is
// called and fails).
int htbl_put(HashTable *ht, KeyType key, ValType value) {
  assert(ht != NULL);
  size_t idx = pr_get_bucket_idx(ht, key);
  ListNode *curr_head = ht->arr[idx];

  ListNode *entry = pr_take_lnode(ht, key, value, curr_head);
  if (entry == NULL) {
    return -1;
  }
  ht->arr[idx] = entry;
  ht->el_ct++;

  return 0;
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
int htbl_get(HashTable *ht, KeyType key, ValType *values, int num_values,
             int *num_results) {
  assert(ht != NULL);
  assert(values != NULL);
  assert(num_results != NULL);
  size_t idx = pr_get_bucket_idx(ht, key);
  ListNode *head = ht->arr[idx];

  *num_results = 0;
  while (head != NULL) {
    ListNode *prev = head;
    head = head->next;
    if (prev->key != key) {
      continue;
    }
    if (*num_results < num_values) {
      values[*num_results] = prev->val;
    }
    (*num_results)++;
  }

  return 0;
}

// This method erases all key-value pairs with a given key from the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if the
// hashtable is not allocated).
int htbl_erase(HashTable *ht, KeyType key) {
  assert(ht != NULL);
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
    ht->el_ct--;
  }

  return 0;
}

size_t htbl_size(HashTable *ht) {
  assert(ht != NULL);
  return ht->el_ct;
}

// This method frees all memory occupied by the hash table.
// It returns an error code, 0 for success and -1 otherwise.
int htbl_deallocate(HashTable *ht) {
  assert(ht != NULL);
  for (size_t idx = 0; idx < ht->arr_len; idx++) {
    ListNode *bucket = ht->arr[idx];
    if (bucket != NULL) {
      lnode_free_entire(bucket);
    }
  }
  free(ht->arr);
  if (ht->mem_pool != NULL) {
    lnode_free_entire(ht->mem_pool);
  }
  free(ht);
  return 0;
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
