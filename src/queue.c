#include "include/queue.h"
#include "include/mem.h"
#include "include/utils.h"
#include <assert.h>
#include <stdio.h>

Queue q_new(size_t with_capacity) {
  assert(with_capacity > 0);
  // Hard coded for simplicity, lazy init not as important as in Vec
  return (Queue){.els = MALLOC(sizeof(Generic) * with_capacity),
                 .cap = with_capacity,
                 .left = 0,
                 .right = 0,
                 .len = 0};
}

void q_free(Queue *q) { free(q->els); }

bool q_full(Queue *q) { return q->len == q->cap; }

void q_push(Queue *q, Generic el) {
  if (q_full(q)) {
    log_err("QUEUE: Cannot push to a full queue. Check q_full first.");
    exit(1);
  }
  q->len++;
  q->els[q->right] = el;
  q->right = (q->right + 1) % q->cap;
}

Generic q_pop(Queue *q) {
  if (q->len == 0) {
    log_err("QUEUE: Cannot pop from an empty queue. Check q_len first.");
    exit(1);
  }
  q->len--;
  size_t idx = q->left;
  q->left = (idx + 1) % q->cap;
  return q->els[idx];
}
