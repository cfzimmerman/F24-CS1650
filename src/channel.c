#include "include/channel.h"
#include "include/mem.h"
#include "include/queue.h"
#include <pthread.h>

// Inspired by rust mpsc channels and informed by this guide:
// https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032r/index.html#sync-44265

/// Creates a new channel with exactly the given capacity. Tx requests when the
/// queue is full will block until there's space in the channel.
Channel chan_new(size_t with_capacity) {
  Channel chan = {.q = q_new(with_capacity)};
  pthread_mutex_init(&chan.lock, NULL);
  pthread_cond_init(&chan.wait_send, NULL);
  pthread_cond_init(&chan.wait_receive, NULL);
  return chan;
}

/// Frees all resources held by the channel. This requires (without checking)
/// that absolutely every reference to the channel has been dropped so that the
/// channel will never be accessed again. Any usage of the channel after
/// chan_free causes undefined behavior.
void chan_free(Channel *chan) {
  pthread_mutex_lock(&chan->lock);
  pthread_cond_destroy(&chan->wait_receive);
  pthread_cond_destroy(&chan->wait_send);
  pthread_mutex_destroy(&chan->lock);
  q_free(&chan->q);
}

/// Receives the next element from the channel. Blocks until a message arrives
/// if the queue is currently empty.
Generic chan_recv(Channel *chan) {
  pthread_mutex_lock(&chan->lock);
  while (chan->q.len == 0) {
    pthread_cond_wait(&chan->wait_receive, &chan->lock);
  }
  Generic el = q_pop(&chan->q);
  pthread_cond_signal(&chan->wait_send);
  pthread_mutex_unlock(&chan->lock);
  return el;
}

/// Sends a message into the channel. If the queue is currently
/// full, blocks until there's space.
void chan_send(Channel *chan, Generic msg) {
  pthread_mutex_lock(&chan->lock);
  while (q_full(&chan->q)) {
    pthread_cond_wait(&chan->wait_send, &chan->lock);
  }
  q_push(&chan->q, msg);
  pthread_cond_signal(&chan->wait_receive);
  pthread_mutex_unlock(&chan->lock);
}

/// Attempts to send a message into the channel. Behaves the same as `chan_send`
/// if the queue is not full. Returns true on successful send.
/// If the queue is full, returns false instead of blocking.
bool chan_try_send(Channel *chan, Generic msg) {
  pthread_mutex_lock(&chan->lock);
  bool pushed = false;
  if (!q_full(&chan->q)) {
    q_push(&chan->q, msg);
    pushed = true;
    pthread_cond_signal(&chan->wait_receive);
  }
  pthread_mutex_unlock(&chan->lock);
  return pushed;
}
