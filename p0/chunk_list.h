
#ifndef CS165_CHUNK_LIST
#define CS165_CHUNK_LIST

#include "generic.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define CHL_ARR_SIZE 6

typedef struct ch_list_node {
  size_t len;
  struct ch_list_node *next;
  Generic arr[CHL_ARR_SIZE];
} ChunkListNode;

inline ChunkListNode *chl_node_new(Generic init_val, ChunkListNode *next) {
  ChunkListNode *node = (ChunkListNode *)malloc(sizeof(ChunkListNode));
  assert(node != NULL);
  node->len = 1;
  node->arr[0] = init_val;
  node->next = next;
  return node;
}

inline void chl_node_free_all(ChunkListNode *head) {
  while (head != NULL) {
    ChunkListNode *prev = head;
    head = head->next;
    free(prev);
  }
}

typedef struct chl_list_iter {
  ChunkListNode *node;
  size_t idx;
} ChunkListIter;

inline ChunkListIter chl_iter_new(ChunkListNode *head) {
  return (ChunkListIter){.node = head, .idx = 0};
}

inline bool chl_iter_next(ChunkListIter *iter, Generic *result) {
  if (iter->node != NULL && iter->idx == iter->node->len) {
    iter->idx = 0;
    iter->node = iter->node->next;
  }
  if (iter->node == NULL) {
    return false;
  }
  *result = iter->node->arr[iter->idx++];
  return true;
}

#endif
