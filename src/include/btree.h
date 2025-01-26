#ifndef CZ_BTREE_H
#define CZ_BTREE_H

#include "mem.h"
#include <stdbool.h>
#include <stddef.h>

/// Design note:
/// I made this before the B+ tree because CLRS had much more
/// readable pseudocode for a B tree. I plan on using the B+ tree
/// in the DB, but I'm keeping this around in case I want to use
/// it for experiments.

/// Corresponds to `t` in CLRS. Change this as needed to match hardware.
#define BTREE_T 16
#define MAX_KEYS_PER_NODE 2 * BTREE_T - 1
#define MAX_CHILDREN_PER_NODE 1 + MAX_KEYS_PER_NODE

typedef struct BtreeNode {
  Generic vals[MAX_KEYS_PER_NODE];
  struct BtreeNode *children[MAX_CHILDREN_PER_NODE];
  size_t key_ct;
  int keys[MAX_KEYS_PER_NODE];
  bool is_leaf;
} BtreeNode;

// A B-tree for integer keys and Generic values.
// Prefer to use BPtree, this was largely a learning exercise.
// Mostly drawn from CLRS chapter 18.
typedef struct {
  BtreeNode *root;
} Btree;

Btree btree_new();

void btree_insert(Btree *tree, int key, Generic val);

OptGeneric btree_get(Btree *tree, int key);

// Only public for testing
void btree_split_child(BtreeNode *parent, size_t child_idx);

void btree_free(Btree *tree);

#endif
