#ifndef CZ_BPTREE_H
#define CZ_BPTREE_H

#include "mem.h"

#define BTREE_T 5
#define MAX_KEYS_PER_NODE (2 * BTREE_T - 1)
#define MAX_CHILDREN_PER_NODE (1 + MAX_KEYS_PER_NODE)

// A node in the BPtree.
typedef struct {
  // Either BPtreeNode* children or Generic values depending
  // on whether this is a leaf.
  Generic meta[MAX_CHILDREN_PER_NODE];
  size_t key_ct;
  int keys[MAX_KEYS_PER_NODE];
  bool is_leaf;
} BPtreeNode;

// B+ tree for integer keys and Generic values.
typedef struct {
  BPtreeNode *root;
} BPtree;

BPtree bptree_new();

void bptree_free(BPtree *tree);

typedef struct {
  int key;
  Generic val;
} BpResult;

// Returns the exact key-value pair if a match was found.
// If not, returns the next largest key-value pair if possible.
// If the next largest key-value pair exceeds the tree's max key,
// return's the tree's max key-value pair.
BpResult bptree_get(BPtree *tree, int key);

void bptree_insert(BPtree *tree, int key, Generic val);

BPtree bptree_new_loaded(int *keys, Generic *vals, size_t len);

#endif
