#include "./include/vector.h"
#include "assert.h"
#include "stdint.h"
#include <stdint.h>
#include <stdio.h>

int main() {
  {
    // Test from empty.
    Vec vec = vec_new(0);
    vec_push(&vec, (Generic){.uint = 50});
    assert(vec.capacity == 4);
    assert(vec.len == 1);
    uint64_t val = vec_pop(&vec).uint;
    assert(val == 50);
    vec_free(&vec);
    assert(vec.len == 0);
  }

  Vec vec = vec_new(3);
  assert(vec.capacity == 4);

  {
    // Test push + pop
    for (int num = 0; num < 20; num++) {
      vec_push(&vec, (Generic){.uint = -num});
      assert((int)vec_index(&vec, num).uint == -num);
    }
    assert(vec.len == 20);
    assert(vec.capacity == 32);

    while (vec.len != 0) {
      int expected = -(vec.len - 1);
      assert(expected == (int)vec_pop(&vec).uint);
    }
    assert(vec.len == 0);
    assert(vec.capacity == 32);
  }

  {
    // Test swap + pop
    vec_push(&vec, (Generic){.uint = 56});
    vec_push(&vec, (Generic){.uint = 57});
    vec_push(&vec, (Generic){.uint = 58});

    vec_swap(&vec, 1, 2);
    assert(vec_pop(&vec).uint == 57);
    assert(vec_index(&vec, 0).uint == 56);
    assert(vec_index(&vec, 1).uint == 58);
    assert(vec.len == 2);
    assert(vec_pop(&vec).uint == 58);
    assert(vec_pop(&vec).uint == 56);
    assert(vec.len == 0);
  }

  {
    vec_push(&vec, (Generic){.uint = 0});
    vec_push(&vec, (Generic){.uint = 1});
    vec_push(&vec, (Generic){.uint = 2});
    vec_push(&vec, (Generic){.uint = 3});

    vec_reverse(&vec);
    assert(vec.arr[0].uint == 3);
    assert(vec.arr[1].uint == 2);
    assert(vec.arr[2].uint == 1);
    assert(vec.arr[3].uint == 0);

    vec_reverse(&vec);
    vec_push(&vec, (Generic){.uint = 4});
    vec_reverse(&vec);
    assert(vec.arr[0].uint == 4);
    assert(vec.arr[1].uint == 3);
    assert(vec.arr[2].uint == 2);
    assert(vec.arr[3].uint == 1);
    assert(vec.arr[4].uint == 0);
  }

  vec_free(&vec);

  printf("✅ %s\n", __FILE__);
  return 0;
}
