#ifndef CZ_QUEUE
#define CZ_QUEUE

#include "mem.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Queue {
  size_t cap;
  size_t left;
  size_t right;
  size_t len;
  Generic *els;
} Queue;

/// Instantiates a new queue with exactly the given capacity. This queue does
/// not dynamically resize, and pushing to it while full will cause a panic.
Queue q_new(size_t with_capacity);

/// Frees resources held by the queue data structure. The API caller is
/// responsible for cleaning up any resources identified by pointers within the
/// queue and the storage of the queue itself.
void q_free(Queue *q);

/// Returns whether the queue is currently full.
bool q_full(Queue *q);

/// Pushes an element into the queue.
///
/// PANICS
/// If the queue is currently full, pushing to it causes a panic. Before
/// pushing, check if it's full with q_full.
void q_push(Queue *q, Generic el);

/// Pops an element from the queue.
///
/// PANICS
/// If the queue is currently empty, popping from it causes a panic. Before
/// popping, check if it's empty with q_len.
Generic q_pop(Queue *q);

#endif
