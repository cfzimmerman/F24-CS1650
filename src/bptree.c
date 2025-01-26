#include "include/bptree.h"
#include "include/mem.h"
#include "include/utils.h"
#include <assert.h>
#include <math.h>
#include <string.h>

// Invariants:
// - In an internal node, meta[idx] contains all keys LEQ keys[idx].
// - Splitting internal nodes causes their midpoint to MOVE up.
// - Splitting leaf nodes causes their midpoint to COPY up.

void bptree_split_child(BPtreeNode *parent, size_t child_idx) {
  assert(child_idx <= parent->key_ct);
  assert(!parent->is_leaf);
  assert(parent->key_ct < MAX_KEYS_PER_NODE);

  BPtreeNode *child = parent->meta[child_idx].ptr;
  assert(child->key_ct == MAX_KEYS_PER_NODE);

  BPtreeNode *new_child = MALLOC(sizeof(BPtreeNode));
  new_child->is_leaf = child->is_leaf;

  // Shift the parent's keys and meta to the right to make
  // space for its new child (lol cute)
  memmove(&parent->keys[child_idx + 1], &parent->keys[child_idx],
          sizeof(int) * (parent->key_ct - child_idx));
  memmove(&parent->meta[child_idx + 2], &parent->meta[child_idx + 1],
          sizeof(Generic) * (parent->key_ct - child_idx));

  // shift this many keys to the new child from the right of child
  new_child->key_ct = BTREE_T - 1;
  memcpy(new_child->keys, &child->keys[BTREE_T],
         sizeof(int) * new_child->key_ct);

  if (child->is_leaf) {
    // Split and copy the middle to the parent. Marks
    // this leaf's rightmost entry.

    // Copy over values
    memcpy(new_child->meta, &child->meta[BTREE_T],
           sizeof(Generic) * (new_child->key_ct));
    child->key_ct = BTREE_T;
  } else {
    // Split and move the middle to the parent.

    // Copy over children
    memcpy(new_child->meta, &child->meta[BTREE_T],
           sizeof(Generic) * (new_child->key_ct + 1));
    child->key_ct = BTREE_T - 1;
  }

  // Put the new key into the parent, update child size
  parent->keys[child_idx] = child->keys[BTREE_T - 1];
  parent->meta[child_idx + 1].ptr = new_child;
  parent->key_ct++;
}

void pr_insert_nonfull(BPtreeNode *node, int key, Generic val) {
  assert(node->key_ct < MAX_KEYS_PER_NODE);

  size_t idx = 0;
  while (idx < node->key_ct && node->keys[idx] < key) {
    // Could do binary search instead.
    idx++;
  }

  if (node->is_leaf) {
    bool key_already_exists = (idx < node->key_ct && node->keys[idx] == key);
    if (!key_already_exists) {
      memmove(&node->keys[idx + 1], &node->keys[idx],
              sizeof(int) * (node->key_ct - idx));
      memmove(&node->meta[idx + 1], &node->meta[idx],
              sizeof(Generic) * (node->key_ct - idx));
      node->key_ct++;
    }
    node->keys[idx] = key;
    node->meta[idx] = val;
    return;
  }

  // `meta[idx]` is the subtree where the key belongs.
  if (((BPtreeNode *)node->meta[idx].ptr)->key_ct == MAX_KEYS_PER_NODE) {
    bptree_split_child(node, idx);
    if (node->keys[idx] < key) {
      idx++;
    }
  }
  pr_insert_nonfull(node->meta[idx].ptr, key, val);
}

void pr_free_node(BPtreeNode *node) {
  if (!node->is_leaf) {
    for (size_t idx = 0; idx < node->key_ct + 1; idx++) {
      pr_free_node(node->meta[idx].ptr);
    }
  }
  free(node);
}

BpResult pr_get(BPtreeNode *node, int key) {
  size_t idx = 0;
  while (idx < node->key_ct && node->keys[idx] < key) {
    // Could binary search here
    idx++;
  }
  if (!node->is_leaf) {
    return pr_get(node->meta[idx].ptr, key);
  }
  if (node->key_ct <= idx) {
    idx--;
  }
  return (BpResult){.key = node->keys[idx], .val = node->meta[idx]};
}

int pr_greatest_key_in_subtree(BPtreeNode *node) {
  if (node->is_leaf) {
    assert(0 < node->key_ct);
    return node->keys[node->key_ct - 1];
  }
  return pr_greatest_key_in_subtree(node->meta[node->key_ct].ptr);
}

BPtree bptree_new_loaded(int *keys, Generic *vals, size_t len) {
  size_t leaf_ct = ceill((double)len / (double)MAX_KEYS_PER_NODE);
  BPtreeNode **nodes = MALLOC(sizeof(BPtreeNode *) * leaf_ct);

  // Make the leaves
  for (size_t idx = 0; idx < leaf_ct; idx++) {
    nodes[idx] = MALLOC(sizeof(BPtreeNode));
    BPtreeNode *node = nodes[idx];
    size_t offset = idx * MAX_KEYS_PER_NODE;

    node->key_ct = min_unsig(MAX_KEYS_PER_NODE, len - offset);
    node->is_leaf = true;

    memcpy(node->keys, &keys[offset], sizeof(int) * node->key_ct);
    memcpy(node->meta, &vals[offset], sizeof(Generic) * node->key_ct);
  }

  // Now build the internal structure
  size_t node_ct = leaf_ct;
  while (1 < node_ct) {
    size_t new_node_ct = 0;
    for (size_t offset = 0; offset < node_ct; offset += MAX_CHILDREN_PER_NODE) {

      BPtreeNode *inner = MALLOC(sizeof(BPtreeNode));
      inner->is_leaf = false;

      size_t child_ct = min_unsig(MAX_CHILDREN_PER_NODE, node_ct - offset);
      assert(child_ct != 0);
      inner->key_ct = child_ct - 1;

      for (size_t child_idx = 0; child_idx < child_ct; child_idx++) {
        inner->meta[child_idx].ptr = nodes[offset + child_idx];
        if (child_idx < inner->key_ct) {
          inner->keys[child_idx] =
              pr_greatest_key_in_subtree(inner->meta[child_idx].ptr);
        }
      }
      nodes[new_node_ct++] = inner;
    }
    node_ct = new_node_ct;
  }

  BPtreeNode *root = nodes[0];
  if (root->is_leaf) {
    BPtreeNode *new_root = MALLOC(sizeof(BPtreeNode));
    new_root->is_leaf = false;
    new_root->key_ct = 0;
    new_root->meta[0] = (Generic){.ptr = root};
    root = new_root;
  }

  free(nodes);
  return (BPtree){.root = root};
}

void bptree_insert(BPtree *tree, int key, Generic val) {
  if (tree->root->key_ct == MAX_KEYS_PER_NODE) {
    BPtreeNode *new_root = MALLOC(sizeof(BPtreeNode));
    new_root->is_leaf = false;
    new_root->key_ct = 0;
    new_root->meta[0].ptr = tree->root;
    tree->root = new_root;
    bptree_split_child(tree->root, 0);
  }
  pr_insert_nonfull(tree->root, key, val);
}

BpResult bptree_get(BPtree *tree, int key) {
  // Reject gets from an empty tree. Indicates a bug.
  assert(((BPtreeNode *)(tree->root->meta[0].ptr))->key_ct > 0);
  return pr_get(tree->root, key);
}

void bptree_free(BPtree *tree) { pr_free_node(tree->root); }

BPtree bptree_new() {
  BPtreeNode *root = MALLOC(sizeof(BPtreeNode));
  BPtreeNode *child = MALLOC(sizeof(BPtreeNode));
  *root = (BPtreeNode){.key_ct = 0, .is_leaf = false};
  *child = (BPtreeNode){.key_ct = 0, .is_leaf = true};
  root->meta[0].ptr = child;
  return (BPtree){.root = root};
}
