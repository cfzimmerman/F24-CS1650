#ifndef CZ_CHANNEL_H
#define CZ_CHANNEL_H

#include "queue.h"
#include <pthread.h>

typedef struct Channel {
  Queue q;
  pthread_mutex_t lock;
  pthread_cond_t wait_send;
  pthread_cond_t wait_receive;
} Channel;

/// Creates a new channel with exactly the given capacity. Tx requests when the
/// queue is full will block until there's space in the channel.
Channel chan_new(size_t with_capacity);

/// Frees all resources held by the channel. This requires (without checking)
/// that absolutely every reference to the channel has been dropped so that the
/// channel will never be accessed again. Any usage of the channel after
/// chan_free causes undefined behavior.
void chan_free(Channel *chan);

/// Receives the next element from the channel. Blocks until a message arrives
/// if the queue is currently empty.
Generic chan_recv(Channel *chan);

/// Sends a message into the channel. If the queue is currently
/// full, blocks until there's space.
void chan_send(Channel *chan, Generic msg);

/// Attempts to send a message into the channel. Behaves the same as `chan_send`
/// if the queue is not full. Returns true on successful send.
/// If the queue is full, returns false instead of blocking.
bool chan_try_send(Channel *chan, Generic msg);

#endif
