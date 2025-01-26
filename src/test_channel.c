#include "include/channel.h"
#include <assert.h>
#include <pthread.h>

#define THREAD_CT 10
#define CHAN_LEN 8

typedef struct TaskArgs {
  Channel *channel;
  size_t total;
} TaskArgs;

void *task(void *args) {
  TaskArgs *input = args;
  for (size_t ct = 0; ct < input->total; ct++) {
    chan_send(input->channel, (Generic){.uint = 1});
  }
  return 0;
}

int main() {
  Channel chan = chan_new(CHAN_LEN);
  pthread_t threads[THREAD_CT];
  TaskArgs args[THREAD_CT];

  for (size_t task_ct = 0; task_ct < THREAD_CT; task_ct++) {
    args[task_ct] = (TaskArgs){.channel = &chan, .total = 47};
    assert(pthread_create(&threads[task_ct], NULL, task, &args[task_ct]) == 0);
  }

  size_t expected = 47 * THREAD_CT;
  size_t total = 0;
  for (size_t ct = 0; ct < expected; ct++) {
    total += chan_recv(&chan).uint;
  }
  assert(total == expected);

  for (size_t idx = 0; idx < THREAD_CT; idx++) {
    assert(pthread_join(threads[idx], NULL) == 0);
  }

  size_t pushed = 0;
  for (size_t ct = 0; ct < 100; ct++) {
    pushed += (size_t)chan_try_send(&chan, (Generic){.uint = ct});
  }
  assert(pushed == CHAN_LEN);

  chan_free(&chan);
  printf("✅ %s\n", __FILE__);
  return 0;
}
