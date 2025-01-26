#include "include/bptree2.h"
#include "include/mem.h"
#include "include/vector.h"
#include <assert.h>
#include <math.h>
#include <string.h>

void bptree2_split_child(BPNodeInternal *parent, size_t child_idx) {
  assert(child_idx <= parent->hd.key_ct);
  assert(!parent->hd.is_leaf);
  assert(parent->hd.key_ct < MAX_KEYS_PER_NODE2);

  void *old_child = parent->children[child_idx];
  BPNodeHeader *old_child_head = old_child;
  assert(old_child_head->key_ct == MAX_KEYS_PER_NODE2);

  void *new_child;
  if (old_child_head->is_leaf) {
    BPNodeLeaf *leaf = MALLOC(sizeof(BPNodeLeaf));
    leaf->hd.is_leaf = true;
    new_child = leaf;
  } else {
    BPNodeInternal *internal = MALLOC(sizeof(BPNodeInternal));
    internal->hd.is_leaf = false;
    new_child = internal;
  }
  BPNodeHeader *new_child_head = new_child;

  // Shift the parent's keys and children to the right to make
  // space for its new child
  memmove(&parent->hd.keys[child_idx + 1], &parent->hd.keys[child_idx],
          sizeof(int) * (parent->hd.key_ct - child_idx));
  memmove(&parent->children[child_idx + 2], &parent->children[child_idx + 1],
          sizeof(void *) * (parent->hd.key_ct - child_idx));

  // shift this many keys to the new child from the right of old child
  new_child_head->key_ct = BTREE2_T - 1;
  memcpy(new_child_head->keys, &old_child_head->keys[BTREE2_T],
         sizeof(int) * new_child_head->key_ct);

  if (new_child_head->is_leaf) {
    assert(old_child_head->is_leaf);
    // Split and copy the middle to the parent. Marks
    // this leaf's rightmost entry.
    BPNodeLeaf *old_leaf = old_child;
    BPNodeLeaf *new_leaf = new_child;

    // Copy over values
    memcpy(new_leaf->vals, &old_leaf->vals[BTREE2_T],
           sizeof(Vec) * (new_child_head->key_ct));
    old_child_head->key_ct = BTREE2_T;

    new_leaf->next = old_leaf->next;
    old_leaf->next = new_leaf;
  } else {
    assert(!old_child_head->is_leaf);
    // Split and move the middle to the parent.
    BPNodeInternal *old_internal = old_child;
    BPNodeInternal *new_internal = new_child;

    // Copy over children
    memcpy(new_internal->children, &old_internal->children[BTREE2_T],
           sizeof(void *) * (new_child_head->key_ct + 1));
    old_child_head->key_ct = BTREE2_T - 1;
  }

  // Put the new key into the parent, update child size
  parent->hd.keys[child_idx] = old_child_head->keys[BTREE2_T - 1];
  parent->children[child_idx + 1] = new_child;
  parent->hd.key_ct++;
}

void pr2_insert_nonfull(void *node_unk, int key, Generic val) {
  BPNodeHeader *node_head = node_unk;
  assert(node_head->key_ct < MAX_KEYS_PER_NODE2);

  size_t idx = 0;
  while (idx < node_head->key_ct && node_head->keys[idx] < key) {
    // Could do binary search instead.
    idx++;
  }

  if (node_head->is_leaf) {
    BPNodeLeaf *leaf = node_unk;
    bool key_already_exists =
        (idx < leaf->hd.key_ct && leaf->hd.keys[idx] == key);
    if (!key_already_exists) {
      memmove(&leaf->hd.keys[idx + 1], &leaf->hd.keys[idx],
              sizeof(int) * (leaf->hd.key_ct - idx));
      memmove(&leaf->vals[idx + 1], &leaf->vals[idx],
              sizeof(Vec) * (leaf->hd.key_ct - idx));
      leaf->hd.key_ct++;
      leaf->vals[idx] = vec_new(2);
    }
    leaf->hd.keys[idx] = key;
    vec_push(&leaf->vals[idx], val);
    return;
  }

  assert(!node_head->is_leaf);
  BPNodeInternal *parent = node_unk;
  if (((BPNodeHeader *)(parent->children[idx]))->key_ct == MAX_KEYS_PER_NODE2) {
    bptree2_split_child(parent, idx);
    if (parent->hd.keys[idx] < key) {
      idx++;
    }
  }
  pr2_insert_nonfull(parent->children[idx], key, val);
}

void pr2_free_node(void *node) {
  bool is_leaf = ((BPNodeHeader *)node)->is_leaf;
  if (is_leaf) {
    BPNodeLeaf *leaf = node;
    for (size_t val_idx = 0; val_idx < leaf->hd.key_ct; val_idx++) {
      vec_free(&leaf->vals[val_idx]);
    }
  } else {
    BPNodeInternal *internal = node;
    for (size_t child_idx = 0; child_idx < internal->hd.key_ct + 1;
         child_idx++) {
      pr2_free_node(internal->children[child_idx]);
    }
  };
  free(node);
}

typedef struct {
  BPNodeLeaf *leaf;
  size_t key_idx;
} LeafPos;

/// If the key is matched, returns a pointer to a vector of values
/// associated with that key.
/// Treat the returned pointer like an &mut reference in Rust.
/// DO NOT access the pointer after modifying the tree again, as vectors
/// shift around inside the tree.
LeafPos pr2_get(void *node, int key) {
  BPNodeHeader *hd = node;
  size_t idx = 0;
  while (idx < hd->key_ct && hd->keys[idx] < key) {
    // Could binary search here
    idx++;
  }
  if (!hd->is_leaf) {
    return pr2_get(((BPNodeInternal *)node)->children[idx], key);
  }
  BPNodeLeaf *leaf = node;
  if (leaf->hd.key_ct <= idx) {
    idx--;
  }
  return (LeafPos){.leaf = leaf, .key_idx = idx};
}

int pr2_greatest_key_in_subtree(void *node) {
  BPNodeHeader *hd = node;
  if (hd->is_leaf) {
    assert(0 < hd->key_ct);
    return hd->keys[hd->key_ct - 1];
  }
  return pr2_greatest_key_in_subtree(
      ((BPNodeInternal *)node)->children[hd->key_ct]);
}

BPtree2 bptree2_new_loaded(int *input_keys, Generic *input_vals,
                           size_t input_len) {
  if (input_len == 0) {
    return bptree2_new();
  }
  int *keys = MALLOC(sizeof(int) * input_len);
  Vec *vals = MALLOC(sizeof(Vec) * input_len);

  size_t kv_idx = 0;
  keys[kv_idx] = input_keys[kv_idx];
  vals[kv_idx] = vec_new(4);

  for (size_t input_idx = 0; input_idx < input_len; input_idx++) {
    if (keys[kv_idx] != input_keys[input_idx]) {
      assert(keys[kv_idx] < input_keys[input_idx]);
      kv_idx++;
      keys[kv_idx] = input_keys[input_idx];
      vals[kv_idx] = vec_new(4);
    }
    assert(keys[kv_idx] == input_keys[input_idx]);
    vec_push(&vals[kv_idx], input_vals[input_idx]);
  }
  size_t kv_len = kv_idx + 1;
  size_t leaf_ct = ceill((double)kv_len / (double)MAX_KEYS_PER_NODE2);

  void **nodes = MALLOC(sizeof(void *) * leaf_ct);
  // Make the leaves
  for (size_t idx = 0; idx < leaf_ct; idx++) {
    nodes[idx] = MALLOC(sizeof(BPNodeLeaf));
    BPNodeLeaf *node = nodes[idx];
    size_t offset = idx * MAX_KEYS_PER_NODE2;

    node->hd.key_ct = min_unsig(MAX_KEYS_PER_NODE2, kv_len - offset);
    node->hd.is_leaf = true;
    if (0 < idx) {
      ((BPNodeLeaf *)nodes[idx - 1])->next = node;
    }

    memcpy(node->hd.keys, &keys[offset], sizeof(int) * node->hd.key_ct);
    memcpy(node->vals, &vals[offset], sizeof(Vec) * node->hd.key_ct);
  }
  ((BPNodeLeaf *)(nodes[leaf_ct - 1]))->next = NULL;

  // Now build the internal structure
  size_t node_ct = leaf_ct;
  while (1 < node_ct) {
    size_t new_node_ct = 0;
    for (size_t offset = 0; offset < node_ct;
         offset += MAX_CHILDREN_PER_NODE2) {
      BPNodeInternal *inner = MALLOC(sizeof(BPNodeInternal));
      inner->hd.is_leaf = false;

      size_t child_ct = min_unsig(MAX_CHILDREN_PER_NODE2, node_ct - offset);
      assert(child_ct != 0);
      inner->hd.key_ct = child_ct - 1;

      for (size_t child_idx = 0; child_idx < child_ct; child_idx++) {
        inner->children[child_idx] = nodes[offset + child_idx];
        if (child_idx < inner->hd.key_ct) {
          inner->hd.keys[child_idx] =
              pr2_greatest_key_in_subtree(inner->children[child_idx]);
        }
      }
      nodes[new_node_ct++] = inner;
    }
    node_ct = new_node_ct;
  }

  void *root = nodes[0];
  if (((BPNodeHeader *)root)->is_leaf) {
    BPNodeInternal *new_root = MALLOC(sizeof(BPNodeInternal));
    new_root->hd.is_leaf = false;
    new_root->hd.key_ct = 0;
    new_root->children[0] = root;
    root = new_root;
  }

  free(nodes);
  // Don't free the vectors within vals. They live in the tree now
  free(vals);
  free(keys);

  return (BPtree2){.root = root};
}

BPLeafIter bptree2_iter(BPtree2 *tree, int start) {
  LeafPos pos = pr2_get(tree->root, start);
  return (BPLeafIter){.node = pos.leaf, .key_idx = pos.key_idx, .val_idx = 0};
}

BPLeafIterItem bptree2_iter_next(BPLeafIter *iter) {
  if (iter->node == NULL) {
    return (BPLeafIterItem){.is_some = false};
  }
  if (iter->key_idx >= iter->node->hd.key_ct) {
    iter->key_idx = 0;
    iter->val_idx = 0;
    iter->node = iter->node->next;
    return bptree2_iter_next(iter);
  }
  if (iter->val_idx >= iter->node->vals[iter->key_idx].len) {
    iter->key_idx++;
    iter->val_idx = 0;
    return bptree2_iter_next(iter);
  }
  return (BPLeafIterItem){
      .is_some = true,
      .key = iter->node->hd.keys[iter->key_idx],
      .val = iter->node->vals[iter->key_idx].arr[iter->val_idx++]};
}

void bptree2_insert(BPtree2 *tree, int key, Generic val) {
  if (tree->root->hd.key_ct == MAX_KEYS_PER_NODE2) {
    BPNodeInternal *new_root = MALLOC(sizeof(BPNodeInternal));
    new_root->hd = (BPNodeHeader){.key_ct = 0, .is_leaf = false};
    new_root->children[0] = tree->root;
    tree->root = new_root;
    bptree2_split_child(tree->root, 0);
  }
  pr2_insert_nonfull(tree->root, key, val);
}

BP2Result bptree2_get(BPtree2 *tree, int key) {
  // return &leaf->vals[idx];
  LeafPos pos = pr2_get(tree->root, key);
  return (BP2Result){.key = pos.leaf->hd.keys[pos.key_idx],
                     .val = &pos.leaf->vals[pos.key_idx]};
}

void bptree2_free(BPtree2 *tree) { pr2_free_node(tree->root); }

BPtree2 bptree2_new() {
  BPNodeInternal *root = MALLOC(sizeof(BPNodeInternal));
  BPNodeLeaf *child = MALLOC(sizeof(BPNodeLeaf));
  *root = (BPNodeInternal){.hd = {.key_ct = 0, .is_leaf = false}};
  *child = (BPNodeLeaf){.hd = {.key_ct = 0, .is_leaf = true}, .next = NULL};
  root->children[0] = child;
  return (BPtree2){.root = root};
}
