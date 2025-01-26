#ifndef CZ_BPTREE2_H
#define CZ_BPTREE2_H

#include "mem.h"
#include "vector.h"

#define BTREE2_T 5
#define MAX_KEYS_PER_NODE2 (2 * BTREE2_T - 1)
#define MAX_CHILDREN_PER_NODE2 (1 + MAX_KEYS_PER_NODE2)

typedef struct {
  uint32_t key_ct;
  int keys[MAX_KEYS_PER_NODE2];
  bool is_leaf;
} BPNodeHeader;

typedef struct {
  BPNodeHeader hd;
  void *children[MAX_CHILDREN_PER_NODE2];
} BPNodeInternal;

typedef struct BPNodeLeaf {
  BPNodeHeader hd;
  Vec vals[MAX_KEYS_PER_NODE2];
  struct BPNodeLeaf *next;
} BPNodeLeaf;

// B+ tree for integer keys and Generic values.
//
// Unlike BPtree, this tree stores values for all duplicate keys
// in vectors held by the leaves
typedef struct {
  BPNodeInternal *root;
} BPtree2;

// An iterator over the values in this tree's leaves.
typedef struct {
  BPNodeLeaf *node;
  size_t key_idx;
  size_t val_idx;
} BPLeafIter;

typedef struct {
  // If false, the other fields are meaningless.
  bool is_some;
  int key;
  Generic val;
} BPLeafIterItem;

BPtree2 bptree2_new();

void bptree2_free(BPtree2 *tree);

typedef struct {
  int key;
  Vec *val;
} BP2Result;

BP2Result bptree2_get(BPtree2 *tree, int key);

void bptree2_insert(BPtree2 *tree, int key, Generic val);

BPtree2 bptree2_new_loaded(int *keys, Generic *vals, size_t len);

BPLeafIterItem bptree2_iter_next(BPLeafIter *iter);

BPLeafIter bptree2_iter(BPtree2 *tree, int start);

#endif
