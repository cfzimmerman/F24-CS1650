#ifndef CS165_HASH_TABLE // This is a header guard. It prevents the header from
                         // being included more than once.
#define CS165_HASH_TABLE

#import <stddef.h>
#include <stdint.h>

typedef int keyType;
typedef int valType;

typedef struct list_node {
  valType val;
  struct list_node *next;
} ListNode;

typedef struct hashtable {
  size_t arr_len;
  ListNode *arr;
  size_t el_ct;
} hashtable;

int allocate(hashtable **ht, int size);
int put(hashtable *ht, keyType key, valType value);
int get(hashtable *ht, keyType key, valType *values, int num_values,
        int *num_results);
int erase(hashtable *ht, keyType key);
int deallocate(hashtable *ht);
uint64_t hash(uint64_t key, uint64_t domain);

#endif
