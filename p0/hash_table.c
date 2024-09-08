#include "hash_table.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/// Allocates a new ListNode on the heap and returns a pointer
/// to it. Returns NULL if malloc failed.
ListNode *new_list_node(KeyType key, ValType val, ListNode *next) {
  ListNode *node = malloc(sizeof(ListNode));
  if (node == NULL) {
    return NULL;
  }
  node->key = key;
  node->val = val;
  node->next = next;
  return node;
}

/// Realloc the table if more than `1/OVERSIZE_FACTOR` buckets
/// in the table are filled.
const uint64_t OVERSIZE_FACTOR = 2;

/// Suggests a size for the hash table based on how many elements it's expected
/// to hold.
inline uint64_t reserve_for_capacity(int with_capacity) {
  return pow(2, ceil(log2(abs(with_capacity) * OVERSIZE_FACTOR)));
}

// Initialize the components of a hashtable.
// The size parameter is the expected number of elements to be inserted.
// This method returns an error code, 0 for success and -1 otherwise (e.g., if
// the parameter passed to the method is not null, if malloc fails, etc).
int allocate(HashTable **ht, int with_capacity) {
  uint64_t size = reserve_for_capacity(with_capacity);

  HashTable *table = malloc(sizeof(HashTable));
  if (table == NULL) {
    return -1;
  }

  size_t bucket_list_size = sizeof(ListNode *) * size;
  ListNode **buckets = malloc(bucket_list_size);
  if (buckets == NULL) {
    free(table);
    return -1;
  }
  memset((void *)buckets, 0, bucket_list_size);

  table->arr = buckets;
  table->arr_len = size;
  table->el_ct = 0;
  table->arr_len_pow2 = (uint64_t)floor(log2(size));

  *ht = table;

  return 0;
}

inline uint64_t get_bucket_idx(HashTable *ht, KeyType key) {
  uint64_t idx = hash((uint64_t)key, ht->arr_len_pow2);
  assert(idx < ht->arr_len);
  return idx;
}

// This method inserts a key-value pair into the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if malloc is
// called and fails).
int put(HashTable *ht, KeyType key, ValType value) {
  size_t idx = get_bucket_idx(ht, key);
  ListNode *curr_head = ht->arr[idx];

  ListNode *entry = new_list_node(key, value, curr_head);
  if (entry == NULL) {
    return -1;
  }
  ht->arr[idx] = entry;

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
int get(HashTable *ht, KeyType key, ValType *values, int num_values,
        int *num_results) {
  (void)ht;
  (void)key;
  (void)values;
  (void)num_values;
  (void)num_results;
  return 0;
}

// This method erases all key-value pairs with a given key from the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if the
// hashtable is not allocated).
int erase(HashTable *ht, KeyType key) {
  (void)ht;
  (void)key;
  return 0;
}

// This method frees all memory occupied by the hash table.
// It returns an error code, 0 for success and -1 otherwise.
int deallocate(HashTable *ht) {
  // This line tells the compiler that we know we haven't used the variable
  // yet so don't issue a warning. You should remove this line once you use
  // the parameter.
  (void)ht;
  return 0;
}

/// From the Fibonacci hashing segment:
/// https://en.wikipedia.org/wiki/Hash_function
const uint64_t MULTIPLIER = 11400714819323198485ULL;
const uint64_t WORD_SIZE = 64;

/// Hashes a 64 bit key into the range `0..(2^domain_pow2)`.
/// Assumes the host machine's word size is 64 bits.
inline uint64_t hash(uint64_t key, uint64_t domain_pow2) {
  return (MULTIPLIER * key) >> (WORD_SIZE - domain_pow2);
}
