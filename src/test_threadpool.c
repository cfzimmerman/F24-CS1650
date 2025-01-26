#include "include/channel.h"
#include "include/threadpool.h"
#include <assert.h>
#include <stdio.h>

typedef struct TestTaskArgs {
  Channel *chan; // Channel<int>
  int num;
} TestTaskArgs;

void task(void *input) {
  TestTaskArgs *args = input;
  chan_send(args->chan, (Generic){.uint = args->num});
}

int main() {
  {
    ThreadPool *pool = tpool_new(6);
    tpool_free(pool);
  }

  {
    ThreadPool *pool = tpool_new(8);
    const size_t MSG_CT = 1000;
    Channel results = chan_new(MSG_CT);

    TestTaskArgs args = {.chan = &results, .num = 2};
    TaskInput input = {.arg = &args, .task = task};

    for (size_t idx = 0; idx < MSG_CT; idx++) {
      tpool_task_run(pool, &input);
    }

    size_t total = 0;
    for (size_t ct = 0; ct < MSG_CT; ct++) {
      total += chan_recv(&results).uint;
    }
    assert(total == MSG_CT * 2);

    chan_free(&results);
    tpool_free(pool);
  }

  printf("✅ %s\n", __FILE__);
  return 0;
}
