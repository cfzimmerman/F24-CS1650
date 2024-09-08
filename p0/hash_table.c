#include "hash_table.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Initialize the components of a hashtable.
// The size parameter is the expected number of elements to be inserted.
// This method returns an error code, 0 for success and -1 otherwise (e.g., if
// the parameter passed to the method is not null, if malloc fails, etc).
int allocate(hashtable **ht, int size) {
  if (size <= 0) {
    return -1;
  }

  hashtable *table = malloc(sizeof(hashtable));
  if (table == NULL) {
    return -1;
  }

  size_t bucket_list_size = sizeof(ListNode) * size;
  ListNode *buckets = malloc(bucket_list_size);
  if (buckets == NULL) {
    free(table);
    return -1;
  }
  memset((void *)buckets, 0, bucket_list_size);

  table->arr = buckets;
  table->arr_len = size;
  table->el_ct = 0;

  *ht = table;

  return 0;
}

// This method inserts a key-value pair into the hash table.
// It returns an error code, 0 for success and -1 otherwise (e.g., if malloc is
// called and fails).
int put(hashtable *ht, keyType key, valType value) {
  (void)ht;
  (void)key;
  (void)value;
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
int get(hashtable *ht, keyType key, valType *values, int num_values,
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
int erase(hashtable *ht, keyType key) {
  (void)ht;
  (void)key;
  return 0;
}

// This method frees all memory occupied by the hash table.
// It returns an error code, 0 for success and -1 otherwise.
int deallocate(hashtable *ht) {
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
uint64_t hash(uint64_t key, uint64_t domain_pow2) {
  return (MULTIPLIER * key) >> (WORD_SIZE - domain_pow2);
}
