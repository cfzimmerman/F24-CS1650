#include "include/btree.h"
#include "include/mem.h"
#include <assert.h>
#include <string.h>

void pr_free_node(BtreeNode *node) {
  if (!node->is_leaf) {
    for (size_t idx = 0; idx < node->key_ct + 1; idx++) {
      pr_free_node(node->children[idx]);
    }
  }
  free(node);
}

// Splits a not-full parent's full child into two children.
// Panics if parent is full.
//
// `child_idx` should be an index in the child array such that
// `parent->children[child_idx]` is the full node to split.
void btree_split_child(BtreeNode *parent, size_t child_idx) {
  assert(child_idx <= parent->key_ct);
  assert(!parent->is_leaf);
  assert(parent->key_ct < MAX_KEYS_PER_NODE);

  BtreeNode *child = parent->children[child_idx];
  assert(child->key_ct == MAX_KEYS_PER_NODE);

  // Transfer the left half of child's keys and values
  // to new_child.
  BtreeNode *new_child = MALLOC(sizeof(BtreeNode));
  new_child->is_leaf = child->is_leaf;
  new_child->key_ct = BTREE_T - 1;
  memcpy(new_child->keys, &child->keys[BTREE_T],
         sizeof(int) * new_child->key_ct);
  memcpy(new_child->vals, &child->vals[BTREE_T],
         sizeof(Generic) * new_child->key_ct);

  if (!child->is_leaf) {
    // Transfer the children corresponding to those keys
    memcpy(new_child->children, &child->children[BTREE_T],
           sizeof(BtreeNode *) * (new_child->key_ct + 1));
  }

  child->key_ct = BTREE_T - 1;

  // Shift the parent's children and keys to the right to make
  // space for its new child (lol cute)
  memmove(&parent->children[child_idx + 2], &parent->children[child_idx + 1],
          sizeof(BtreeNode *) * (parent->key_ct - child_idx));
  parent->children[child_idx + 1] = new_child;

  memmove(&parent->keys[child_idx + 1], &parent->keys[child_idx],
          sizeof(int) * (parent->key_ct - child_idx));
  memmove(&parent->vals[child_idx + 1], &parent->vals[child_idx],
          sizeof(Generic) * (parent->key_ct - child_idx));

  parent->keys[child_idx] = child->keys[BTREE_T - 1];
  parent->vals[child_idx] = child->vals[BTREE_T - 1];
  parent->key_ct++;
}

void pr_insert_nonfull(BtreeNode *node, int key, Generic val) {
  // This departs from the CLRS pseudocode because I want to
  // guarantee unique keys in my tree.

  assert(node->key_ct < MAX_KEYS_PER_NODE);

  size_t idx = 0;
  while (idx < node->key_ct && node->keys[idx] < key) {
    // Could do binary search instead.
    idx++;
  }

  if (idx < node->key_ct && node->keys[idx] == key) {
    // Duplicate key
    node->vals[idx] = val;
    return;
  }

  if (node->is_leaf) {
    // `keys[idx]` is where the new key should be inserted.
    memmove(&node->keys[idx + 1], &node->keys[idx],
            sizeof(int) * (node->key_ct - idx));
    memmove(&node->vals[idx + 1], &node->vals[idx],
            sizeof(Generic) * (node->key_ct - idx));

    node->keys[idx] = key;
    node->vals[idx] = val;
    node->key_ct++;

    return;
  }

  // `children[idx]` is the subtree where the key belongs.
  if (node->children[idx]->key_ct == MAX_KEYS_PER_NODE) {
    btree_split_child(node, idx);
    if (node->keys[idx] < key) {
      idx++;
    } else if (node->keys[idx] == key) {
      // Splitting the node brought a duplicate key up to the current node.
      node->vals[idx] = val;
      return;
    }
  }

  pr_insert_nonfull(node->children[idx], key, val);
}

OptGeneric pr_get(BtreeNode *node, int key) {
  size_t idx = 0;
  while (idx < node->key_ct && node->keys[idx] < key) {
    // Could binary search here
    idx++;
  }
  if (idx < node->key_ct && node->keys[idx] == key) {
    return generic_some(node->vals[idx]);
  }
  if (node->is_leaf) {
    return generic_none();
  }
  return pr_get(node->children[idx], key);
}

Btree btree_new() {
  BtreeNode *root = MALLOC(sizeof(BtreeNode));
  *root = (BtreeNode){.key_ct = 0, .is_leaf = true};
  return (Btree){.root = root};
}

void btree_free(Btree *tree) { pr_free_node(tree->root); }

void btree_insert(Btree *tree, int key, Generic val) {
  if (tree->root->key_ct == MAX_KEYS_PER_NODE) {
    BtreeNode *new_root = MALLOC(sizeof(BtreeNode));
    new_root->is_leaf = false;
    new_root->key_ct = 0;
    new_root->children[0] = tree->root;
    tree->root = new_root;
    btree_split_child(tree->root, 0);
  }
  pr_insert_nonfull(tree->root, key, val);
}

OptGeneric btree_get(Btree *tree, int key) { return pr_get(tree->root, key); }
