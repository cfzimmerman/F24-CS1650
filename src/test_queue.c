#include "include/queue.h"
#include <assert.h>

int main() {
  {
    // [18, 14, 11, 12, 14, 15, 19, 10, 15, 10]
    Queue q = q_new(4);
    assert(q.len == 0);

    q_push(&q, (Generic){.uint = 18});
    q_push(&q, (Generic){.uint = 14});
    q_push(&q, (Generic){.uint = 11});
    q_push(&q, (Generic){.uint = 12});

    assert(q.len == 4);
    assert(q_pop(&q).uint == 18);
    assert(q_pop(&q).uint == 14);

    q_push(&q, (Generic){.uint = 14});
    q_push(&q, (Generic){.uint = 15});

    assert(q_full(&q));
    assert(q_pop(&q).uint == 11);

    q_push(&q, (Generic){.uint = 19});

    assert(q_full(&q));
    assert(q_pop(&q).uint == 12);
    assert(q_pop(&q).uint == 14);
    assert(q_pop(&q).uint == 15);
    assert(q_pop(&q).uint == 19);

    q_push(&q, (Generic){.uint = 10});

    assert(q_pop(&q).uint == 10);

    q_push(&q, (Generic){.uint = 15});
    q_push(&q, (Generic){.uint = 10});

    assert(q.len == 2);
    assert(q_pop(&q).uint == 15);
    assert(q_pop(&q).uint == 10);
    assert(q.len == 0);

    q_free(&q);
  }

  printf("✅ %s\n", __FILE__);
  return 0;
}
