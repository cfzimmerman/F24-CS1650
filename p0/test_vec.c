#include "assert.h"
#include "stdint.h"
#include "vector.h"

int main() {
  {
    // Test from empty.
    Vec vec = vec_new(0);
    vec_push(&vec, (Generic){.unsig = 50});
    assert(vec.capacity == 4);
    assert(vec.len == 1);
    uint64_t val = vec_pop(&vec).unsig;
    assert(val == 50);
    vec_free(&vec);
    assert(vec.len == 0);
  }

  Vec vec = vec_new(3);
  assert(vec.capacity == 4);

  {
    // Test push + pop
    for (int num = 0; num < 20; num++) {
      vec_push(&vec, (Generic){.sig = -num});
      assert(vec_index(&vec, num).sig == -num);
    }
    assert(vec.len == 20);
    assert(vec.capacity == 32);

    while (vec.len != 0) {
      int expected = -(vec.len - 1);
      assert(expected == vec_pop(&vec).sig);
    }
    assert(vec.len == 0);
    assert(vec.capacity == 32);
  }

  {
    // Test swap + pop
    vec_push(&vec, (Generic){.unsig = 56});
    vec_push(&vec, (Generic){.unsig = 57});
    vec_push(&vec, (Generic){.unsig = 58});

    vec_swap(&vec, 1, 2);
    assert(vec_pop(&vec).unsig == 57);
    assert(vec_index(&vec, 0).unsig == 56);
    assert(vec_index(&vec, 1).unsig == 58);
    assert(vec.len == 2);
  }

  vec_free(&vec);
  return 0;
}
