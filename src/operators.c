#include "include/operators.h"
#include "include/api2.h"
#include "include/bitvec.h"
#include "include/catalogue.h"
#include "include/client_context.h"
#include "include/mem.h"
#include "include/parse2.h"
#include "include/utils.h"
#include "include/vector.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

Status db_insert_row(Catalogue *ctlg, char *db_name, char *table_name,
                     Vec *vals) {
  if (ctlg->db == NULL || strcmp(ctlg->db->name, db_name) != 0) {
    return status_err("INSERT ROW: db_name does not match ctlg.db_name");
  }

  Table2 *tbl = ctlg_find_table(ctlg->db, table_name);
  if (tbl == NULL) {
    return status_err("INSERT ROW: table name not found");
  }
  if (tbl->columns.len != vals->len) {
    return status_err("INSERT ROW: len(vals) != len(cols)");
  }
  if (tbl->indexes_valid) {
    // NOTE: indices currently don't rebuild after a point insertion. This
    // may cause silent errors and is a low-priority to-do.
    //
    // log_err("to do: Unexpected row insertion invalidated index: %s\n",
    //         table_name);
    tbl->indexes_valid = false;
  }

  for (size_t idx = 0; idx < tbl->columns.len; idx++) {
    col_push(db_name, table_name, (Column2 *)(tbl->columns.arr[idx].ptr),
             vals->arr[idx].uint);
  }
  return status_ok();
}

ChandleIntVec col_fetch_slice(Column2 *col, ChandleSlice *slice) {
  int *res = MALLOC(sizeof(int) * slice->len);
  memcpy(res, &col->data[slice->start_idx], sizeof(int) * slice->len);
  return (ChandleIntVec){.len = slice->len, .nums = res};
}

ChandleIntVec col_fetch_poslist(Column2 *col, ChandlePosList *positions) {
  int *res = MALLOC(sizeof(int) * positions->len);
  for (size_t idx = 0; idx < positions->len; idx++) {
    res[idx] = col->data[positions->pos[idx]];
  }
  return (ChandleIntVec){.len = positions->len, .nums = res};
}

FetchResult db_fetch(Catalogue *ctlg, ClientCxt2 *cxt, char *db, char *table,
                     char *column, char *chname) {
  Chandle *chandle = chandle_get(cxt, chname);
  if (chandle == NULL) {
    return (FetchResult){.status =
                             status_err("Requested chandle does not exist")};
  }
  FoundColumn maybe_col = find_column(ctlg, db, table, column);
  if (maybe_col.status.code == ERROR) {
    return (FetchResult){.status = maybe_col.status};
  }

  switch (chandle->type) {
  case CHANDLE_BITVEC: {
    ChandleIntVec results =
        col_fetch_bitvec(maybe_col.col, &chandle->val.bv.bitvec);
    return (FetchResult){.status = status_ok(), .results = results};
  }
  case CHANDLE_SLICE: {
    ChandleIntVec results = col_fetch_slice(maybe_col.col, &chandle->val.slice);
    return (FetchResult){.status = status_ok(), .results = results};
  }
  case CHANDLE_POSLIST: {
    ChandleIntVec results = col_fetch_poslist(maybe_col.col, &chandle->val.pos);
    return (FetchResult){.status = status_ok(), .results = results};
  }
  case CHANDLE_AGGREGATE:
  case CHANDLE_INTVEC:
  default: {
    return (FetchResult){
        .status = status_err("Fetch handle must be the output of select")};
  }
  }
}

PrintResult pr_print_aggregates(ClientCxt2 *cxt, Vec *printable) {
  char *res = MALLOC(MAX_CHARS_PER_INT * printable->len + 64);
  size_t res_offset = 0;
  res[0] = '\0';

  for (size_t idx = 0; idx < printable->len; idx++) {
    ParsePrintable *parsed = printable->arr[idx].ptr;
    if (parsed->type != PRINT_FETCHED) {
      return (PrintResult){
          .status = status_err("FAILED: cannot print selected bitvecs")};
    }
    Chandle *chandle = chandle_get(cxt, parsed->val.fetched.chandle);
    if (chandle->type != CHANDLE_AGGREGATE) {
      return (PrintResult){
          .status = status_err("FAILED: aggregates can be printed together but "
                               "not mixed with other results")};
    }
    switch (chandle->val.agg.prec) {
    case AGG_REAL: {
      res_offset += snprintf(&res[res_offset], MAX_CHARS_PER_INT, "%.2f,",
                             chandle->val.agg.val);
      break;
    }
    case AGG_INTEGER: {
      res_offset += snprintf(&res[res_offset], MAX_CHARS_PER_INT, "%.20g,",
                             chandle->val.agg.val);
      break;
    }
    }
  }
  // Overwrite last comma to a terminator
  if (res_offset > 0) {
    res[res_offset - 1] = '\0';
  }

  return (PrintResult){.status = status_ok(),
                       .str = (SaferStr){.type = STR_DYNAMIC, .str = res}};
}

PrintResult db_print(Catalogue *ctlg, ClientCxt2 *cxt, Vec *printable) {
  if (printable->len == 0) {
    return (PrintResult){.status = status_ok(),
                         .str = (SaferStr){.str = "", .type = STR_STATIC}};
  }

  // db_print cares about lists of values. Use print_aggregates to handle
  // printing single values.
  ParsePrintable *first = printable->arr[0].ptr;
  if (first->type == PRINT_FETCHED) {
    Chandle *chandle = chandle_get(cxt, first->val.fetched.chandle);
    if (chandle != NULL && chandle->type == CHANDLE_AGGREGATE) {
      return pr_print_aggregates(cxt, printable);
    }
  }

  // Holds a pointer to either a column or fetched array. Regardless, every
  // array must be the same length.
  // Note this is an array of arrays.
  int **cols = MALLOC(sizeof(int *) * printable->len);
  // Every column in cols needs to be this length.
  size_t len_of_each_col = 0;

  for (size_t idx = 0; idx < printable->len; idx++) {
    ParsePrintable *parsed = printable->arr[idx].ptr;
    size_t col_len = 0;
    int *col_vals = NULL;

    switch (parsed->type) {
    case PRINT_COLUMN: {
      FoundColumn col = find_column(ctlg, parsed->val.column.db_name,
                                    parsed->val.column.table_name,
                                    parsed->val.column.col_name);
      if (col.status.code == ERROR) {
        free(cols);
        return (PrintResult){.status = col.status};
      }
      col_len = col.col->len;
      col_vals = col.col->data;
      break;
    }
    case PRINT_FETCHED: {
      Chandle *chandle = chandle_get(cxt, parsed->val.fetched.chandle);
      if (chandle == NULL) {
        free(cols);
        return (PrintResult){.status =
                                 status_err("Failed to retrieve chandle")};
      }
      if (chandle->type != CHANDLE_INTVEC) {
        free(cols);
        return (PrintResult){
            .status = status_err("Bulk print only supports intvecs")};
      }
      col_len = chandle->val.iv.len;
      col_vals = chandle->val.iv.nums;
      break;
    }
    default: {
      log_err("Missing case in db_print\n");
      exit(1);
    }
    }

    assert(col_vals != NULL);
    if (idx == 0) {
      len_of_each_col = col_len;
    }
    if (col_len != len_of_each_col) {
      free(cols);
      return (PrintResult){
          .status = status_err(
              "Only columns with the same length can be printed together")};
    }
    cols[idx] = col_vals;
  }

  size_t output_size = 256;
  char *output = MALLOC(output_size);
  size_t out_idx = 0;
  for (size_t col_idx = 0; col_idx < len_of_each_col; col_idx++) {
    for (size_t arr_idx = 0; arr_idx < printable->len; arr_idx++) {
      char num_buf[MAX_CHARS_PER_INT];
      size_t val_size =
          snprintf(num_buf, MAX_CHARS_PER_INT, "%d,", cols[arr_idx][col_idx]);
      if (output_size < out_idx + val_size + 4) {
        output = REALLOC(output, (output_size *= 2));
      }
      assert(val_size < MAX_CHARS_PER_INT);
      memcpy(&output[out_idx], num_buf, val_size);
      out_idx += val_size;
    }
    if (out_idx > 0) {
      // Overwrite the last comma into a newline
      output[out_idx - 1] = '\n';
    }
  }

  if (out_idx > 0) {
    // Overwrite the last newline into a terminator
    output[out_idx - 1] = '\0';
  } else {
    output[0] = '\0';
  }
  free(cols);

  return (PrintResult){.status = status_ok(),
                       .str = (SaferStr){.str = output, .type = STR_DYNAMIC}};
}

Status db_load(Catalogue *ctlg, char *db_name, char *table_name,
               Vec *cols /*Vec<&CsvColumn> */) {
  if (ctlg->db == NULL) {
    return status_err("FAILED: Cannot load into a nonexistent db");
  }
  if (strcmp(ctlg->db->name, db_name) != 0) {
    return status_err("FAILED: Load db name is different from the current db");
  }

  Table2 *table = ctlg_find_table(ctlg->db, table_name);
  if (table == NULL) {
    return status_err("FAILED: Table does not exist");
  }

  if (table->columns.len != cols->len) {
    return status_err(
        "FAILED: CSV has a different number of cols than the table");
  }

  // Match each column before beginning insertions in case there's a stray that
  // doesn't belong here.
  // Assumes the parser takes care of deduplication.
  for (size_t col_idx = 0; col_idx < cols->len; col_idx++) {
    CsvColumn *input = cols->arr[col_idx].ptr;
    Column2 *col = ctlg_find_column(table, input->col_name);
    if (col == NULL) {
      return status_err("FAILED: column name not found");
    }
  }

  tbl_invalidate_indexes(table);

  assert(cols->len > 0);
  for (size_t col_idx = 0; col_idx < cols->len; col_idx++) {
    CsvColumn *input = cols->arr[col_idx].ptr;
    Column2 *col = ctlg_find_column(table, input->col_name);
    assert(col != NULL); // Just checked above, this definitely exists.

    for (size_t val_idx = 0; val_idx < input->vals.len; val_idx++) {
      Status st = col_push(db_name, table_name, col,
                           (int)input->vals.arr[val_idx].uint);
      if (st.code == ERROR) {
        // failure recovery can be added later
        // **cringe**
        log_err("col_push failed, db is now corrupted: %s\n", st.error_message);
        exit(1);
      }
    }
  }

  return status_ok();
}

ChandleIntVec col_fetch_bitvec(Column2 *col, BitVec *inputs) {
  int *res = MALLOC(sizeof(int) * inputs->bit_ones);
  size_t res_idx = 0;

  for (size_t major_bit = 0; major_bit < inputs->bit_len; major_bit += 8) {
    if (__builtin_expect(res_idx == inputs->bit_ones, false)) {
      // Exit early if all selected values have been materialized.
      break;
    }
    uint8_t byte = inputs->bits[major_bit / 8];
    uint8_t minor_bit = 0;
    while (byte != 0) {
      if (byte & 1) {
        res[res_idx] = col->data[major_bit + minor_bit];
        res_idx += 1;
      }
      // res[res_idx] = col->data[major_bit + minor_bit];
      // res_idx += (byte & 1);
      byte >>= 1;
      minor_bit++;

      // Locally, the if statement above seems to outperform the branchless
      // version that touches all memory no matter what.
      //

      // Safety: In the case where col.len % 8 != 0, this code looks
      // suspiciously prone to buffer overread. However, in that case, the
      // remaining bits in the bitvec will always be zero, so the while loop
      // will short circuit out of the loop before any out of bounds accesses
      // are made.
    }
  }

  return (ChandleIntVec){.nums = res, .len = res_idx};
}

Status col_push(char *db_name, char *table_name, Column2 *column, int value) {
  if (__builtin_expect(column->len < column->cap, true)) {
    column->data[column->len++] = value;
    return status_ok();
  }

  // Realloc: unmap current, open file, double its size, mmap the file again,
  // close the file.

  size_t curr_file_bytes = sizeof(int) * column->cap;
  if (munmap(column->data, curr_file_bytes) < 0) {
    perror("munmap");
    log_err("Failed to realloc the db file!\n");
    return status_err("COLUMN PUSH: failed");
  }

  // NOTE: Failures after this point leave the db in an unexpected state, so I'd
  // rather just kill and address error recovery later.

  char *filepath = ctlg_path_column(db_name, table_name, column->name);
  int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("open");
    log_err("Failed to create a new column file at %s\n", filepath);
    free(filepath);
    exit(1);
  }

  size_t new_file_bytes = curr_file_bytes * 2;
  if (ftruncate(fd, new_file_bytes) < 0) {
    perror("ftruncate");
    log_err("Failed to increase the size of %s\n", filepath);
    free(filepath);
    exit(1);
  }

  int *data =
      mmap(NULL, new_file_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  free(filepath);
  if (data == MAP_FAILED) {
    perror("mmap");
    log_err("Failed to mmap file\n");
    exit(1);
  }

  column->cap = new_file_bytes / sizeof(int);
  column->data = data;

  column->data[column->len++] = value;
  return status_ok();
}

int64_t sum_agg(int *nums, size_t len) {
  uint64_t total = 0;
  for (size_t idx = 0; idx < len; idx++) {
    total += nums[idx];
  }
  return total;
}

double avg_agg(int *nums, size_t len) {
  if (len == 0) {
    return 0.;
  }
  double total = sum_agg(nums, len);
  return total / (double)len;
}

AggNums pr_get_agg_nums(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req) {
  switch (req->type) {
  case AGG_CHANDLE: {
    Chandle *chandle = chandle_get(cxt, req->val.chandle.chname);
    if (chandle == NULL) {
      return (AggNums){.status = status_err("FAILED: chandle not found")};
    }
    if (chandle->type != CHANDLE_INTVEC) {
      return (AggNums){
          .status = status_err("FAILED: aggregates require an intvec chandle")};
    }
    return (AggNums){.status = status_ok(),
                     .len = chandle->val.iv.len,
                     .nums = chandle->val.iv.nums};
  }
  case AGG_COLUMN: {
    FoundColumn col =
        find_column(ctlg, req->val.column.db_name, req->val.column.table_name,
                    req->val.column.col_name);
    if (col.status.code == ERROR) {
      return (AggNums){.status = col.status};
    }
    return (AggNums){
        .status = status_ok(), .len = col.col->len, .nums = col.col->data};
  }
  }
  log_err("Missed case in get_agg_nums\n");
  exit(1);
}

AggRes db_sum(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req) {
  AggNums nums = pr_get_agg_nums(ctlg, cxt, req);
  if (nums.status.code == ERROR) {
    return (AggRes){.status = nums.status};
  }
  int64_t added = sum_agg(nums.nums, nums.len);
  return (AggRes){.status = status_ok(), .val = added};
}

AggRes db_avg(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req) {
  AggNums nums = pr_get_agg_nums(ctlg, cxt, req);
  if (nums.status.code == ERROR) {
    return (AggRes){.status = nums.status};
  }
  double avg = avg_agg(nums.nums, nums.len);
  return (AggRes){.status = status_ok(), .val = avg};
}

/// Performs the actual computation behind `db_add`, but left and right
/// MUST be the same length. Otherwise this will panic via assertion.
ChandleIntVec col_add(AggNums *left, AggNums *right) {
  assert(left->len == right->len);
  int *res = MALLOC((sizeof(int) * left->len));
  for (size_t idx = 0; idx < left->len; idx++) {
    // Maybe someday try setting one column and then adding the
    // other. Are these fighting for cache space?
    res[idx] = left->nums[idx] + right->nums[idx];
  }
  return (ChandleIntVec){.nums = res, .len = left->len};
}

FetchResult db_add(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *left,
                   ParseAggregate *right) {
  AggNums left_nums = pr_get_agg_nums(ctlg, cxt, left);
  if (left_nums.status.code == ERROR) {
    return (FetchResult){.status = left_nums.status};
  }

  AggNums right_nums = pr_get_agg_nums(ctlg, cxt, right);
  if (right_nums.status.code == ERROR) {
    return (FetchResult){.status = right_nums.status};
  }

  if (left_nums.len != right_nums.len) {
    return (FetchResult){
        .status =
            status_err("db add left and right must have the same length")};
  }
  ChandleIntVec added = col_add(&left_nums, &right_nums);
  return (FetchResult){.status = status_ok(), .results = added};
}

/// Performs the actual computation behind `db_sub`, but left and right
/// MUST be the same length. Otherwise this will panic via assertion.
ChandleIntVec col_sub(AggNums *left, AggNums *right) {
  assert(left->len == right->len);
  int *res = MALLOC((sizeof(int) * left->len));
  for (size_t idx = 0; idx < left->len; idx++) {
    // Maybe someday try setting one column and then adding the
    // other. Are these fighting for cache space?
    res[idx] = left->nums[idx] - right->nums[idx];
  }
  return (ChandleIntVec){.nums = res, .len = left->len};
}

FetchResult db_sub(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *left,
                   ParseAggregate *right) {
  AggNums left_nums = pr_get_agg_nums(ctlg, cxt, left);
  if (left_nums.status.code == ERROR) {
    return (FetchResult){.status = left_nums.status};
  }

  AggNums right_nums = pr_get_agg_nums(ctlg, cxt, right);
  if (right_nums.status.code == ERROR) {
    return (FetchResult){.status = right_nums.status};
  }

  if (left_nums.len != right_nums.len) {
    return (FetchResult){
        .status =
            status_err("db add left and right must have the same length")};
  }
  ChandleIntVec sub = col_sub(&left_nums, &right_nums);
  return (FetchResult){.status = status_ok(), .results = sub};
}

int min_agg(int *nums, size_t len) {
  int min = INT_MAX;
  for (size_t idx = 0; idx < len; idx++) {
    min = min_sig(nums[idx], min);
  }
  return min;
}

int max_agg(int *nums, size_t len) {
  int max = INT_MIN;
  for (size_t idx = 0; idx < len; idx++) {
    max = max_sig(nums[idx], max);
  }
  return max;
}

AggRes db_min_single(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req) {
  AggNums nums = pr_get_agg_nums(ctlg, cxt, req);
  if (nums.status.code == ERROR) {
    return (AggRes){.status = nums.status};
  }
  int min = min_agg(nums.nums, nums.len);
  return (AggRes){.status = status_ok(), .val = min};
}

AggRes db_max_single(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req) {
  AggNums nums = pr_get_agg_nums(ctlg, cxt, req);
  if (nums.status.code == ERROR) {
    return (AggRes){.status = nums.status};
  }
  int max = max_agg(nums.nums, nums.len);
  return (AggRes){.status = status_ok(), .val = max};
}
