#include "include/api2.h"
#include "include/bitvec.h"
#include "include/client_context.h"
#include "include/mem.h"
#include "include/operators.h"
#include "include/threadpool.h"
#include "include/utils.h"
#include "include/vector.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void test_select_and_fetch(ThreadPool *threads) {
  int inputs[] = {1, 0, -5, 287, -2, 4, 5, 5, 1, 3};
  Column2 col = (Column2){.data = inputs,
                          .len = sizeof(inputs) / sizeof(int),
                          .cap = sizeof(inputs) / sizeof(int)};
  BatchSelectQuery query = bsel_query_new(col.len, 0, 0);

  {
    // Select within [-2, 3]
    query.min_incl = -2;
    query.max_incl = 3;

    col_select_batch(&col, &query, 1, threads, threads->thread_ct);

    assert(query.result.bit_ones == 5);
    uint8_t expected[] = {1, 1, 0, 0, 1, 0, 0, 0, 1, 1};
    assert(query.result.bit_len == sizeof(expected) / sizeof(uint8_t));
    for (size_t bit = 0; bit < query.result.bit_len; bit++) {
      uint8_t mask = 1u << (bit % 8);
      uint8_t found = (query.result.bits[bit / 8] & mask) != 0;
      assert(expected[bit] == found);
    }

    ChandleIntVec fetched = col_fetch_bitvec(&col, &query.result);
    assert(fetched.len == query.result.bit_ones);
    assert(fetched.nums[0] == 1);
    assert(fetched.nums[1] == 0);
    assert(fetched.nums[2] == -2);
    assert(fetched.nums[3] == 1);
    assert(fetched.nums[4] == 3);
    free(fetched.nums);
  }

  {
    // Select everything
    query.min_incl = INT_MIN;
    query.max_incl = INT_MAX;
    col_select_batch(&col, &query, 1, threads, threads->thread_ct);
    assert(query.result.bit_ones == 10);
    assert(query.result.bit_len == sizeof(inputs) / sizeof(int));
    for (size_t bit = 0; bit < query.result.bit_len; bit++) {
      uint8_t mask = 1u << (bit % 8);
      uint8_t found = (query.result.bits[bit / 8] & mask) != 0;
      assert(found == 1);
    }

    ChandleIntVec fetched = col_fetch_bitvec(&col, &query.result);
    assert(fetched.len == query.result.bit_ones);
    for (size_t idx = 0; idx < col.len; idx++) {
      assert(fetched.nums[idx] == col.data[idx]);
    }
    free(fetched.nums);
  }

  {
    // Select only 0
    query.min_incl = 0;
    query.max_incl = 0;
    col_select_batch(&col, &query, 1, threads, threads->thread_ct);
    assert(query.result.bit_ones == 1);
    assert(query.result.bit_len == sizeof(inputs) / sizeof(int));
    for (size_t bit = 0; bit < query.result.bit_len; bit++) {
      uint8_t mask = 1u << (bit % 8);
      uint8_t found = (query.result.bits[bit / 8] & mask) != 0;
      if (bit == 1) {
        assert(found == 1);
      } else {
        assert(found == 0);
      }
    }

    ChandleIntVec fetched = col_fetch_bitvec(&col, &query.result);
    assert(fetched.len == query.result.bit_ones);
    assert(fetched.nums[0] == 0);
    free(fetched.nums);
  }

  {
    // Assume select and fetch are correct
    query.min_incl = -2;
    query.max_incl = 4;
    col_select_batch(&col, &query, 1, threads, threads->thread_ct);
    ChandleIntVec fetched = col_fetch_bitvec(&col, &query.result);

    int64_t summed = sum_agg(fetched.nums, fetched.len);
    assert(summed == 7);

    double avg = avg_agg(fetched.nums, fetched.len);
    // Average is 1.167, do this to avoid float equality.
    assert((int)(avg * 100) == 116);

    int min = min_agg(fetched.nums, fetched.len);
    assert(min == -2);

    int max = max_agg(fetched.nums, fetched.len);
    assert(max == 4);

    free(fetched.nums);
  }

  {
    AggNums input =
        (AggNums){.status = status_ok(), .len = col.len, .nums = col.data};
    // immutable borrow
    ChandleIntVec res = col_add(&input, &input);
    assert(res.len == input.len);
    for (size_t idx = 0; idx < col.len; idx++) {
      assert(col.data[idx] * 2 == res.nums[idx]);
    }

    free(res.nums);
  }

  {
    query.min_incl = INT_MIN;
    query.max_incl = INT_MAX;
    col_select_batch(&col, &query, 1, threads, threads->thread_ct);
    ChandleIntVec fetched = col_fetch_bitvec(&col, &query.result);

    // Select from selected (all)
    BitVec results2 = chname_select_bitvec(&query.result, &fetched, 287, 287);
    assert(results2.bit_ones == 1);
    assert(results2.bits[0] == 1 << 3);
    assert(results2.bits[1] == 0);

    bitvec_free(&results2);
    free(fetched.nums);
  }

  {
    query.min_incl = -5;
    query.max_incl = 0;
    col_select_batch(&col, &query, 1, threads, threads->thread_ct);
    ChandleIntVec fetched = col_fetch_bitvec(&col, &query.result);

    // Select from selected (negatives)
    BitVec results2 = chname_select_bitvec(&query.result, &fetched, -6, -1);
    assert(results2.bit_ones == 2);
    assert(results2.bits[0] == ((1 << 2) | (1 << 4)));
    assert(results2.bits[1] == 0);

    bitvec_free(&results2);
    free(fetched.nums);
  }

  bsel_query_free(&query);
}

void test_multi_select(ThreadPool *threads) {
  int inputs[10] = {-3, -11, -6, -5, -17, 12, 7, 6, -9, -11};
  Column2 col = (Column2){.data = inputs,
                          .len = sizeof(inputs) / sizeof(int),
                          .cap = sizeof(inputs) / sizeof(int)};

  BatchSelectQuery queries[3] = {bsel_query_new(col.len, -20, 20),
                                 bsel_query_new(col.len, -2, -2),
                                 bsel_query_new(col.len, -9, 9)};
  col_select_batch(&col, queries, 3, threads, threads->thread_ct);

  {
    BitVec *res1 = &queries[0].result;
    assert(res1->bit_ones == 10);
    assert(res1->bit_len == 10);
    // All 10 should be one
    assert(res1->bits[0] == UINT8_MAX);
    assert(res1->bits[1] == (UINT8_MAX >> 6));
  }

  {
    BitVec *res2 = &queries[1].result;
    assert(res2->bit_ones == 0);
    assert(res2->bit_len == 10);
    // All 10 should be zero
    assert(res2->bits[0] == 0);
    assert(res2->bits[1] == 0);
  }

  {
    BitVec *res3 = &queries[2].result;
    assert(res3->bit_ones == 6);
    assert(res3->bit_len == 10);

    // Filter output looks like this: 11001101 00000001
    // The bits are filled in this order: 87654321, so read the
    // results within a given bit backwards.
    assert(res3->bits[0] == 205);
    assert(res3->bits[1] == 1);
  }

  for (size_t idx = 0; idx < 3; idx++) {
    bsel_query_free(&queries[idx]);
  }
}

void test_primary_sort() {
  Table2 tbl;
  strcpy(tbl.name, "test_table");
  tbl.columns = vec_new(4);

  for (size_t idx = 0; idx < 4; idx++) {
    Column2 *col = MALLOC(sizeof(Column2));
    col->len = 1000;
    col->idx = (Index){.type = IDX_NONE, .val = {.empty = NULL}};
    col->cap = col->len;
    col->data = MALLOC(sizeof(int) * col->len);
    snprintf(col->name, MAX_SIZE_NAME, "column_%lu", idx);
    vec_push(&tbl.columns, (Generic){.ptr = col});
  }

  const size_t PRIMARY_COL = 2;
  Column2 *primary = tbl.columns.arr[PRIMARY_COL].ptr;
  for (size_t row = 0; row < primary->len; row++) {
    primary->data[row] = rand() % 10;
  }

  size_t scale = 1;
  for (size_t col_idx = 0; col_idx < tbl.columns.len; col_idx++) {
    if (col_idx == PRIMARY_COL) {
      continue;
    }
    Column2 *col = tbl.columns.arr[col_idx].ptr;
    for (size_t row = 0; row < col->len; row++) {
      col->data[row] = -1 * (primary->data[row] * (int)(pow(10, scale)));
    }
    scale++;
  }

  // for (size_t idx = 0; idx < tbl.columns.len; idx++) {
  //   Column2 *col = tbl.columns.arr[idx].ptr;
  //   printf("%s: [", col->name);
  //   for (size_t row = 0; row < col->len; row++) {
  //     printf("%d,", col->data[row]);
  //   }
  //   printf("]\n");
  // }

  tbl_sort_primary(&tbl, primary);

  scale = 1;
  for (size_t col_idx = 0; col_idx < tbl.columns.len; col_idx++) {
    if (col_idx == PRIMARY_COL) {
      for (size_t row = 1; row < primary->len; row++) {
        assert(primary->data[row - 1] <= primary->data[row]);
      }
      continue;
    }
    Column2 *col = tbl.columns.arr[col_idx].ptr;
    for (size_t row = 0; row < col->len; row++) {
      assert(col->data[row] == -1 * primary->data[row] * (int)(pow(10, scale)));
    }
    scale++;
  }

  for (size_t col_idx = 0; col_idx < tbl.columns.len; col_idx++) {
    Column2 *col = tbl.columns.arr[col_idx].ptr;
    free(col->data);
    free(col);
  }
  vec_free(&tbl.columns);
}

void test_convert_poslist(ThreadPool *threads) {
  {
    // bitvec to poslist
    int nums[] = {0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0};
    Column2 col = {.data = nums,
                   .len = sizeof(nums) / sizeof(int),
                   .idx = {.type = IDX_NONE, .val = {.empty = NULL}}};
    col.cap = col.len;
    copy_name(col.name, "test_column");

    BatchSelectQuery query = bsel_query_new(col.len, 1, 1);
    col_select_batch(&col, &query, 1, threads, threads->thread_ct);

    // Don't free because the chandle takes over the query bitvec

    Chandle ch = {.type = CHANDLE_BITVEC,
                  .val = {.bv = {.column = &col, .bitvec = query.result}}};
    copy_name(ch.chname, "test_chname");

    Status res = chandle_try_poslist(&ch);
    assert(res.code == OK);
    assert(ch.type == CHANDLE_POSLIST);
    ChandlePosList *pos = &ch.val.pos;

    uint64_t expected[] = {1, 3, 4, 6, 11};
    assert(sizeof(expected) / sizeof(uint64_t) == pos->len);

    for (size_t idx = 0; idx < pos->len; idx++) {
      assert(pos->pos[idx] == expected[idx]);
    }
    assert(strcmp(ch.chname, "test_chname") == 0);

    chandle_free(&ch);
  }

  {
    Chandle ch = {.type = CHANDLE_SLICE,
                  .val = {.slice = {.start_idx = 14, .len = 8}}};
    Status res = chandle_try_poslist(&ch);
    assert(res.code == OK);
    assert(ch.type == CHANDLE_POSLIST);

    ChandlePosList *pos = &ch.val.pos;
    uint64_t expected[] = {14, 15, 16, 17, 18, 19, 20, 21};
    assert((sizeof(expected) / sizeof(uint64_t)) == pos->len);

    for (size_t idx = 0; idx < pos->len; idx++) {
      assert(pos->pos[idx] == expected[idx]);
    }

    chandle_free(&ch);
  }
}

void test_stats(size_t len) {
  int *col = MALLOC(sizeof(int) * len);
  for (size_t idx = 0; idx < len; idx++) {
    col[idx] = idx;
  }
  ColumnStats stats = stats_new(col, len);
  size_t bucket_prob = (1. / STATS_LEN) * 100;

  assert(100 * stats_selectivity(&stats, -100, -1) == 0);
  assert(100 * stats_selectivity(&stats, len, len) == 0);

  assert((size_t)(100 * stats_selectivity(&stats, 0, 0)) == bucket_prob);
  assert((size_t)(100 * stats_selectivity(&stats, len - 1, len - 1)) ==
         bucket_prob);

  assert(100 * stats_selectivity(&stats, 0, len) == 100);

  {
    size_t quarter = len / 4.;
    size_t sel = 100 * stats_selectivity(&stats, quarter, quarter * 3);
    assert(50 - bucket_prob <= sel);
    assert(sel <= 50 + bucket_prob);
  }

  {
    size_t tenth = len / 10.;
    size_t sel = 100 * stats_selectivity(&stats, tenth * 7, tenth * 8);
    assert(10 - bucket_prob - 1 <= sel);
    assert(sel <= 10 + bucket_prob + 1);
  }

  free(col);
}

int main() {
  test_primary_sort();
  test_stats(100);

  ThreadPool *threads = tpool_new(tpool_suggest_size());
  test_select_and_fetch(threads);
  test_multi_select(threads);
  test_convert_poslist(threads);
  tpool_free(threads);

  printf("✅ %s\n", __FILE__);
  return 0;
}
