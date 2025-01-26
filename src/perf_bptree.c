#include "include/bptree.h"
#include "include/bptree2.h"
#include "include/sorting.h"
#include <assert.h>
#include <sys/time.h>

volatile int black_box;

void sim_bptree(int *keys, Generic *vals, size_t len) {
  BPtree tree = bptree_new_loaded(keys, vals, len);

  struct timeval stop, start;
  gettimeofday(&start, NULL);

  for (size_t idx = 0; idx < len; idx++) {
    BpResult got = bptree_get(&tree, keys[idx]);
    assert(got.key == keys[idx]);
    black_box = got.val.uint;
  }

  gettimeofday(&stop, NULL);
  double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 +
                (double)(stop.tv_sec - start.tv_sec);
  printf("bptree get %lu: %f secs\n", len, secs);

  bptree_free(&tree);
}

void sim_bptree2(int *keys, Generic *vals, size_t len) {
  BPtree2 tree = bptree2_new_loaded(keys, vals, len);

  struct timeval stop, start;
  gettimeofday(&start, NULL);

  for (size_t idx = 0; idx < len; idx++) {
    BP2Result got = bptree2_get(&tree, keys[idx]);
    assert(got.key == keys[idx]);
    black_box = got.val->arr[0].uint + 1;
  }

  gettimeofday(&stop, NULL);
  double secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 +
                (double)(stop.tv_sec - start.tv_sec);
  printf("bptree2 get %lu: %f secs\n", len, secs);

  bptree2_free(&tree);
}

// Results on O3 compilation:
//
// bptree get 150000000: 5.143510 secs
// bptree2 get 150000000: 17.327958 secs
//
// (And reversed)
// bptree2 get 150000000: 17.936670 secs
// bptree get 150000000: 5.095869 secs
int main() {
  const size_t LEN = 150000000;
  int *data = MALLOC(sizeof(int) * LEN);
  Generic *vals = MALLOC(sizeof(Generic) * LEN);

  for (size_t idx = 0; idx < LEN; idx++) {
    data[idx] = rand();
    vals[idx].uint = idx;
  }

  quicksort(data, vals, LEN);
  printf("finished setup\n");

  sim_bptree(data, vals, LEN);
  sim_bptree2(data, vals, LEN);

  printf("✨ %s\n", __FILE__);
  return 0;
}
