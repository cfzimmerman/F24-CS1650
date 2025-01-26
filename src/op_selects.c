#include "include/api2.h"
#include "include/operators.h"
#include "include/sorting.h"
#include "include/utils.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

// Extends operators.c with index operations and select operators

ChandlePosList chname_select_slice(const ChandleSlice *filter,
                                   const ChandleIntVec *intvec, int min_incl,
                                   int max_incl) {
  assert(filter->len == intvec->len);
  assert(filter->len > 0);

  ChandlePosList res = {.len = 0,
                        .pos = MALLOC(sizeof(uint64_t) * filter->len)};

  for (size_t filter_idx = 0; filter_idx < filter->len; filter_idx++) {
    bool is_match = min_incl <= intvec->nums[filter_idx] &&
                    intvec->nums[filter_idx] <= max_incl;
    if (is_match) {
      res.pos[res.len++] = filter->start_idx + filter_idx;
    }
  }

  return res;
}

ChandleSlice col_select_clustered_sorted(const Column2 *col, int min_incl,
                                         int max_incl) {
  assert(col->idx.type == IDX_CLUSTERED_SORTED);
  const SortedIdx *index = &col->idx.val.sorted;
  assert(index->len > 0);

  for (size_t idx = 1; idx < index->len; idx++) {
    assert(index->keys[idx - 1] <= index->keys[idx]);
  }

  size_t col_min_idx = col->len;
  if (min_incl <= index->keys[index->len - 1]) {
    size_t pos_min_idx = binary_search(index->keys, index->len, min_incl);
    col_min_idx = index->pos[pos_min_idx].uint;
  }

  size_t col_max_idx = col->len;
  if (max_incl < index->keys[index->len - 1]) {
    size_t pos_max_idx = binary_search(index->keys, index->len, max_incl + 1);
    col_max_idx = index->pos[pos_max_idx].uint;
  }

  return (ChandleSlice){.start_idx = col_min_idx,
                        .len = col_max_idx - col_min_idx};
}

ChandleSlice col_select_clustered_btree(Column2 *col, int min_incl,
                                        int max_incl) {
  assert(col->idx.type == IDX_CLUSTERED_BTREE);
  BPtree *tree = &col->idx.val.clustered_tree;

  size_t col_min_idx = bptree_get(tree, min_incl).val.uint;
  size_t col_max_idx = col->len;
  if (max_incl < INT_MAX) {
    BpResult max = bptree_get(tree, max_incl + 1);
    if (max_incl < max.key) {
      col_max_idx = max.val.uint;
    }
  }

  return (ChandleSlice){.start_idx = col_min_idx,
                        .len = col_max_idx - col_min_idx};
}

ChandlePosList col_select_unclustered_sorted(const Column2 *col, int min_incl,
                                             int max_incl) {
  assert(col->idx.type == IDX_UNCLUSTERED_SORTED);
  const SortedIdx *index = &col->idx.val.sorted;
  assert(index->len == col->len);

  size_t pos_min_idx = index->len;
  if (min_incl <= index->keys[index->len - 1]) {
    pos_min_idx = binary_search(index->keys, index->len, min_incl);
  }

  size_t pos_max_idx = index->len;
  if (max_incl < index->keys[index->len - 1]) {
    pos_max_idx = binary_search(index->keys, index->len, max_incl + 1);
  }
  size_t len = pos_max_idx - pos_min_idx;
  assert(pos_min_idx + len <= index->len);

  ChandlePosList res = {.len = len, .pos = MALLOC(sizeof(uint64_t) * len)};
  memcpy(res.pos, &index->pos[pos_min_idx], sizeof(uint64_t) * res.len);
  quicksort_u64(res.pos, res.len);

  return res;
}

ChandlePosList col_select_unclustered_btree(Column2 *col, int min_incl,
                                            int max_incl) {
  assert(col->idx.type == IDX_UNCLUSTERED_BTREE);
  BPtree2 *tree = &col->idx.val.unclustered_tree;

  Vec res = vec_new(64);

  BPLeafIter iter = bptree2_iter(tree, min_incl);
  BPLeafIterItem next = bptree2_iter_next(&iter);
  if (next.is_some && min_incl <= next.key) {
    while (next.is_some && next.key <= max_incl) {
      vec_push(&res, next.val);
      next = bptree2_iter_next(&iter);
    }
  }

  uint64_t *pos = (uint64_t *)res.arr;
  size_t len = res.len;
  quicksort_u64(pos, len);

  return (ChandlePosList){.pos = pos, .len = len};
}

SelectResult db_select_chname(ClientCxt2 *cxt, char *filter_name,
                              char *intvec_vals, int min_incl, int max_incl) {
  Chandle *filter = chandle_get(cxt, filter_name);
  if (filter == NULL) {
    return (SelectResult){.status =
                              status_err("bitvec filter chandle not found")};
  }

  Chandle *iv_vals = chandle_get(cxt, intvec_vals);
  if (iv_vals == NULL) {
    return (SelectResult){.status =
                              status_err("intvec vals chandle not found")};
  }
  if (iv_vals->type != CHANDLE_INTVEC) {
    return (SelectResult){
        .status = status_err("named val chandle was not an intvec")};
  }

  switch (filter->type) {
  case CHANDLE_BITVEC: {
    BitVec res_bv = chname_select_bitvec(&filter->val.bv.bitvec,
                                         &iv_vals->val.iv, min_incl, max_incl);
    return (SelectResult){
        .status = status_ok(),
        .chandle = {.type = CHANDLE_BITVEC,
                    .val = {.bv = {.bitvec = res_bv,
                                   .column = filter->val.bv.column}}}};
  }
  case CHANDLE_SLICE: {
    ChandlePosList res = chname_select_slice(
        &filter->val.slice, &iv_vals->val.iv, min_incl, max_incl);
    return (SelectResult){
        .status = status_ok(),
        .chandle = {.type = CHANDLE_POSLIST, .val = {.pos = res}}};
  }
  case CHANDLE_INTVEC:
  case CHANDLE_POSLIST:
  case CHANDLE_AGGREGATE:
  default: {
    log_err("Tried to select an unsupported chname %d\n", filter->type);
    return (SelectResult){
        .status = status_err("named filter chandle was not a bitvec")};
  }
  }
}

SelectResult db_select_col(Catalogue *ctlg, char *db_name, char *table_name,
                           char *col_name, int min_incl, int max_incl,
                           ThreadPool *threads, size_t thread_ct) {
  FoundColumn maybe_col = find_column(ctlg, db_name, table_name, col_name);
  if (maybe_col.status.code == ERROR) {
    return (SelectResult){.status = maybe_col.status};
  }
  Column2 *col = maybe_col.col;

  // Finding the column means we found the table just fine.
  Table2 *table = ctlg_find_table(ctlg->db, table_name);
  assert(table);
  IndexType index_type = IDX_NONE;

  if (table->indexes_valid) {
    index_type = col->idx.type;

    // Avoid using a secondary index if selectivity is high
    if (index_type == IDX_UNCLUSTERED_BTREE ||
        index_type == IDX_UNCLUSTERED_SORTED) {
      float sel = stats_selectivity(&col->idx.stats, min_incl, max_incl);
      if (SMOOTH_SCAN_SEL < sel) {
        index_type = IDX_NONE;
      }
    }
  }

  switch (index_type) {
  case IDX_CLUSTERED_SORTED: {
    ChandleSlice res =
        col_select_clustered_sorted(maybe_col.col, min_incl, max_incl);
    return (SelectResult){
        .status = status_ok(),
        .chandle = {.type = CHANDLE_SLICE, .val = {.slice = res}}};
  }
  case IDX_CLUSTERED_BTREE: {
    ChandleSlice res =
        col_select_clustered_btree(maybe_col.col, min_incl, max_incl);
    return (SelectResult){
        .status = status_ok(),
        .chandle = {.type = CHANDLE_SLICE, .val = {.slice = res}}};
  }
  case IDX_UNCLUSTERED_SORTED: {
    ChandlePosList res =
        col_select_unclustered_sorted(maybe_col.col, min_incl, max_incl);
    return (SelectResult){
        .status = status_ok(),
        .chandle = {.type = CHANDLE_POSLIST, .val = {.pos = res}}};
  }
  case IDX_UNCLUSTERED_BTREE: {
    ChandlePosList res =
        col_select_unclustered_btree(maybe_col.col, min_incl, max_incl);
    return (SelectResult){
        .status = status_ok(),
        .chandle = {.type = CHANDLE_POSLIST, .val = {.pos = res}}};
  }
  case IDX_NONE:
  default: {
    BatchSelectQuery query =
        bsel_query_new(maybe_col.col->len, min_incl, max_incl);
    col_select_batch(maybe_col.col, &query, 1, threads, thread_ct);
    return (SelectResult){.status = status_ok(),
                          .chandle = {.type = CHANDLE_BITVEC,
                                      .val = {.bv = {.column = maybe_col.col,
                                                     .bitvec = query.result}}}};
  }
  }
}

// Sorts all rows in a table around a primary column.
// Creates a row tuple with every column other than the primary
// key. Sorts the primary column with the tuples as values.
// Overwrites the entire table with the ordered results
void tbl_sort_primary(Table2 *tbl, Column2 *primary) {
  assert(tbl->columns.len > 0);
  size_t tuple_len = tbl->columns.len - 1;
  Generic *tuples = MALLOC(sizeof(Generic) * primary->len);

  // one [tuple_len] sized chunk of ints for each row in the column
  int *buf = MALLOC(sizeof(int) * tuple_len * primary->len);
  for (size_t row = 0; row < primary->len; row++) {
    tuples[row].ptr = buf + (row * tuple_len);
  }

  size_t tuple_idx = 0;
  for (size_t col_idx = 0; col_idx < tbl->columns.len; col_idx++) {
    Column2 *col = tbl->columns.arr[col_idx].ptr;
    if (strcmp(col->name, primary->name) == 0) {
      assert(col == primary);
      continue;
    }
    assert(col->len == primary->len);
    for (size_t row = 0; row < col->len; row++) {
      int *tuple = tuples[row].ptr;
      tuple[tuple_idx] = col->data[row];
    }
    tuple_idx++;
  }
  assert(tuple_idx == tbl->columns.len - 1);

  int *primary_buf = MALLOC(sizeof(int) * primary->len);
  memcpy(primary_buf, primary->data, sizeof(int) * primary->len);

  quicksort(primary_buf, tuples, primary->len);

  tuple_idx = 0;
  for (size_t col_idx = 0; col_idx < tbl->columns.len; col_idx++) {
    Column2 *col = tbl->columns.arr[col_idx].ptr;
    if (col == primary) {
      for (size_t row = 0; row < primary->len; row++) {
        col->data[row] = primary_buf[row];
      }
    } else {
      for (size_t row = 0; row < primary->len; row++) {
        int *tuple = tuples[row].ptr;
        col->data[row] = tuple[tuple_idx];
      }
      tuple_idx++;
    }
  }
  assert(tuple_idx == tbl->columns.len - 1);

  free(tuples);
  free(buf);
  free(primary_buf);
}

/// Thread pool worker args for pr_col_select_chunk
typedef struct {
  int *nums;
  size_t batch_len;
  size_t start_idx;
  BatchSelectQuery *queries;
  size_t *bit_ones; // same length as queries
  size_t num_queries;
  Channel *finished;
} ChunkSelectInput;

/// Runs selection over a chunk of column data for a batch of queries.
/// This is designed for running in a thread pool worker.
void pr_col_select_chunk(void *input) {
  ChunkSelectInput *args = input;
  assert(args->start_idx % 8 == 0);

  size_t col_stop = args->start_idx + args->batch_len;
  // For each query
  for (size_t col_idx = args->start_idx; col_idx < col_stop; col_idx += 8) {
    for (size_t query_idx = 0; query_idx < args->num_queries; query_idx++) {
      BatchSelectQuery *query = &args->queries[query_idx];
      uint8_t new_byte = 0;

      // Compute the select results of those 8 entries
      size_t seg_len = (col_stop - col_idx) >= 8 ? 8 : (col_stop - col_idx);
      for (size_t bit_idx = 0; bit_idx < seg_len; bit_idx++) {
        int num = args->nums[col_idx + bit_idx];
        uint8_t new_bit = query->min_incl <= num && num <= query->max_incl;
        new_byte |= (new_bit << bit_idx);
        args->bit_ones[query_idx] += new_bit;
      }

      // Every task writes to its own segment of the result bits, so this won't
      // cause UB.
      query->result.bits[col_idx / 8] = new_byte;
    }
  }

  chan_send(args->finished, (Generic){.uint = 1});
}

void col_select_batch(Column2 *col, BatchSelectQuery *queries,
                      size_t num_queries, ThreadPool *threads,
                      size_t thread_ct) {
  assert(thread_ct <= threads->thread_ct);
  for (size_t idx = 0; idx < num_queries; idx++) {
    bitvec_reserve(&queries[idx].result, col->len);
    queries[idx].result.bit_ones = 0;
    queries[idx].result.bit_len = col->len;
  }

  size_t l1_alloc_ints = 8000;
  size_t l2_alloc_ints = thread_ct * l1_alloc_ints;
  assert(l1_alloc_ints % 8 == 0);

  ChunkSelectInput task_args[thread_ct];
  TaskInput task_configs[thread_ct];

  size_t bit_ones[thread_ct][num_queries];
  memset(bit_ones, 0, sizeof(size_t) * thread_ct * num_queries);

  Channel finished = chan_new(thread_ct);

  for (size_t offset = 0; offset < col->len; offset += l2_alloc_ints) {
    size_t tasks_spawned = 0;
    for (size_t chunk_idx = 0; chunk_idx < thread_ct; chunk_idx++) {
      size_t chunk_start = offset + l1_alloc_ints * chunk_idx;
      if (col->len <= chunk_start) {
        break;
      }
      size_t len = min_unsig(col->len - chunk_start, l1_alloc_ints);
      assert(0 < len && len <= col->len);

      task_args[chunk_idx] = (ChunkSelectInput){
          .nums = col->data,
          .start_idx = chunk_start,
          .batch_len = len,
          .queries = queries,
          .num_queries = num_queries,
          .bit_ones = bit_ones[chunk_idx],
          .finished = &finished,
      };
      task_configs[chunk_idx] = (TaskInput){.arg = &task_args[chunk_idx],
                                            .task = pr_col_select_chunk};
      tpool_task_run(threads, &task_configs[chunk_idx]);
      tasks_spawned++;
    }

    for (size_t ct = 0; ct < tasks_spawned; ct++) {
      chan_recv(&finished);
    }

    // once thread_ct tasks have returned their results, it's safe to proceed
    // to the next batch.
  }

  for (size_t thread_idx = 0; thread_idx < thread_ct; thread_idx++) {
    for (size_t query_idx = 0; query_idx < num_queries; query_idx++) {
      queries[query_idx].result.bit_ones += bit_ones[thread_idx][query_idx];
    }
  }

  chan_free(&finished);
}

BatchSelectQuery bsel_query_new(size_t column_len, int min_incl, int max_incl) {
  return (BatchSelectQuery){.result = bitvec_new(column_len),
                            .min_incl = min_incl,
                            .max_incl = max_incl};
}

void bsel_query_free(BatchSelectQuery *query) { bitvec_free(&query->result); }

BatchSelectQueryResult db_select_batch(Catalogue *ctlg, char *db_name,
                                       char *tbl_name, char *col_name,
                                       Vec *filters /* Vec<SelectColFilter> */,
                                       ThreadPool *threads, size_t thread_ct) {
  // Practically infinite right now. This will likely need to be dynamic
  // and/or tuned.
  const size_t MAX_QUERIES_PER_BATCH = 32;

  FoundColumn maybe_col = find_column(ctlg, db_name, tbl_name, col_name);
  if (maybe_col.status.code == ERROR) {
    return (BatchSelectQueryResult){
        .status = status_err(maybe_col.status.error_message)};
  }
  Column2 *col = maybe_col.col;

  BatchSelectQuery *queries = MALLOC(sizeof(BatchSelectQuery) * filters->len);
  for (size_t idx = 0; idx < filters->len; idx++) {
    SelectColFilter *filter = filters->arr[idx].ptr;
    queries[idx] = bsel_query_new(col->len, filter->min_incl, filter->max_incl);
  }

  for (size_t batch_start = 0; batch_start < filters->len;
       batch_start += MAX_QUERIES_PER_BATCH) {
    size_t batch_end =
        min_unsig(filters->len, batch_start + MAX_QUERIES_PER_BATCH);
    col_select_batch(col, &queries[batch_start], batch_end - batch_start,
                     threads, thread_ct);
  }

  return (BatchSelectQueryResult){
      .status = status_ok(), .col = col, .queries = queries};
}

BitVec chname_select_bitvec(const BitVec *base_filter,
                            const ChandleIntVec *intvec, int min_incl,
                            int max_incl) {
  BitVec bv = bitvec_new(base_filter->bit_len);
  size_t iv_idx = 0;

  for (size_t major_bit = 0; major_bit < base_filter->bit_len; major_bit += 8) {
    uint8_t prev_byte = base_filter->bits[major_bit / 8];
    uint8_t new_byte = 0;

    for (size_t minor_bit = 0; minor_bit < 8 && prev_byte != 0; minor_bit++) {
      uint8_t cond = (min_incl <= intvec->nums[iv_idx] &&
                      intvec->nums[iv_idx] <= max_incl);
      // only 1 or zero because of cond
      new_byte |= ((prev_byte & cond) << minor_bit);
      iv_idx += (prev_byte & 1);
      bv.bit_ones += (prev_byte & cond);
      prev_byte >>= 1;
    }

    bv.bits[major_bit / 8] = new_byte;
  }
  bv.bit_len = base_filter->bit_len;
  return bv;
}

SaferStr db_rebuild_indexes(Catalogue *ctlg, char *db_name, char *table_name) {
  Db2 *db = ctlg->db;
  if (db == NULL || strcmp(db->name, db_name) != 0) {
    return (SaferStr){.type = STR_STATIC,
                      .str = "Index rebuild failed: invalid database name"};
  }
  Table2 *table = ctlg_find_table(db, table_name);
  if (table == NULL) {
    return (SaferStr){.type = STR_STATIC,
                      .str = "Index rebuild failed: Table not found"};
  }
  tbl_rebuild_indexes(table, true);
  return (SaferStr){.type = STR_STATIC, .str = ""};
}

void tbl_rebuild_indexes(Table2 *table, bool re_sort) {
  // First invalidate anything existing.
  if (table->indexes_valid) {
    return;
  }
  if (table->columns.len == 0 ||
      ((Column2 *)table->columns.arr[0].ptr)->len == 0) {
    return;
  }

  // Sort the table on the primary column if needed
  if (re_sort) {
    for (size_t col_idx = 0; col_idx < table->columns.len; col_idx++) {
      Column2 *col = table->columns.arr[col_idx].ptr;
      if (col->idx.type == IDX_CLUSTERED_BTREE ||
          col->idx.type == IDX_CLUSTERED_SORTED) {
        tbl_sort_primary(table, col);
        break;
      }
    }
  }

  for (size_t col_idx = 0; col_idx < table->columns.len; col_idx++) {
    Column2 *col = table->columns.arr[col_idx].ptr;
    IndexType idx_type = col->idx.type;

    if (idx_type == IDX_NONE) {
      continue;
    }

    assert(col->len > 0);
    assert(idx_type == IDX_CLUSTERED_SORTED ||
           idx_type == IDX_CLUSTERED_BTREE ||
           idx_type == IDX_UNCLUSTERED_BTREE ||
           idx_type == IDX_UNCLUSTERED_SORTED);

    int *keys = MALLOC(sizeof(int) * col->len);
    memcpy(keys, col->data, sizeof(int) * col->len);
    Generic *pos = MALLOC(sizeof(Generic) * col->len);
    for (size_t row = 0; row < col->len; row++) {
      pos[row].uint = row;
    }

    size_t final_len = col->len;
    if (idx_type == IDX_UNCLUSTERED_BTREE ||
        idx_type == IDX_UNCLUSTERED_SORTED) {
      quicksort(keys, pos, col->len);
      col->idx.stats = stats_new(keys, col->len);
    } else if (idx_type == IDX_CLUSTERED_BTREE ||
               idx_type == IDX_CLUSTERED_SORTED) {
      // Clustered index's column is already sorted
      final_len = dedup_uint(keys, pos, col->len);
    }

    switch (idx_type) {
    case IDX_CLUSTERED_SORTED:
    case IDX_UNCLUSTERED_SORTED: {
      col->idx.val.sorted =
          (SortedIdx){.keys = keys, .pos = pos, .len = final_len};
      break;
    }
    case IDX_CLUSTERED_BTREE: {
      col->idx.val.clustered_tree = bptree_new_loaded(keys, pos, final_len);
      free(keys);
      free(pos);
      break;
    }
    case IDX_UNCLUSTERED_BTREE: {
      col->idx.val.unclustered_tree = bptree2_new_loaded(keys, pos, final_len);
      free(keys);
      free(pos);
      break;
    }
    case IDX_NONE:
    default: {
      log_err("Missed case in rebuild_index: %d\n", idx_type);
      exit(1);
    }
    }
  }
  table->indexes_valid = true;
}

void tbl_invalidate_indexes(Table2 *table) {
  if (!table->indexes_valid) {
    return;
  }
  table->indexes_valid = false;
  for (size_t idx = 0; idx < table->columns.len; idx++) {
    Column2 *col = table->columns.arr[idx].ptr;
    if (col->len == 0) {
      return;
    }

    switch (col->idx.type) {
    case IDX_CLUSTERED_BTREE: {
      bptree_free(&col->idx.val.clustered_tree);
      break;
    }
    case IDX_UNCLUSTERED_BTREE: {
      bptree2_free(&col->idx.val.unclustered_tree);
      break;
    }
    case IDX_CLUSTERED_SORTED:
    case IDX_UNCLUSTERED_SORTED: {
      free(col->idx.val.sorted.keys);
      free(col->idx.val.sorted.pos);
      break;
    }
    case IDX_NONE:
    default: {
    }
    }
  }
}

ColumnStats stats_new(int *sorted_col, size_t len) {
  if (len == 0) {
    return stats_empty();
  }
  ColumnStats stats;
  float stride = len / (double)(STATS_LEN - 1);
  for (size_t bucket = 0; bucket < STATS_LEN; bucket++) {
    size_t idx = bucket * stride;
    int key = idx < len ? sorted_col[idx] : sorted_col[len - 1];
    stats.histogram[bucket] = key;
  }
  return stats;
}

ColumnStats stats_empty() {
  ColumnStats stats;
  memset(stats.histogram, 0, sizeof(int) * STATS_LEN);
  return stats;
}

float stats_selectivity(ColumnStats *stats, int min_incl, int max_incl) {
  assert(min_incl <= max_incl);
  if (max_incl < stats->histogram[0] ||
      stats->histogram[STATS_LEN - 1] < min_incl) {
    return 0.;
  }

  size_t min_idx = binary_search(stats->histogram, STATS_LEN, min_incl);
  size_t max_idx = binary_search(stats->histogram, STATS_LEN, max_incl);

  return (max_idx - min_idx + 1) / (float)STATS_LEN;
}
