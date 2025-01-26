#include "include/parse2.h"
#include "include/api2.h"
#include "include/mem.h"
#include "include/utils.h"
#include "include/vector.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/// Finds the first instance of delim in query and sets it to \0.
/// Returns the number of bytes to jump from the start of query to
/// reach the first byte after the new null terminator (check for OOB!).
///
/// Returns 0 if there was no match.
size_t next_token(char *query, char delim) {
  if (query == NULL) {
    return 0;
  }
  char *token = strchr(query, delim);
  if (token == NULL) {
    // log_err("next_token: delim '%c' not found in '%s'\n", delim, query);
    return 0;
  }
  *token = '\0';
  return token - query + 1;
}

/// If a string has length greater than MAX_SIZE_NAME, truncates the
/// string to have that length with a null terminator in the last byte.
static inline void truncate_name(char *name) {
  if (name != NULL && strlen(name) >= MAX_SIZE_NAME) {
    name[MAX_SIZE_NAME - 1] = '\0';
  }
}

// General:
//
// Most of these parsing functions work by extracting
// all inputs and then writing them into the struct at the very
// end. An early return leaves cmd untouched.
//
// Most parsing modifies the command in-place by seeking forward
// for separator characters and then inserting null characters to
// create distinct words. Those words are then copied into name
// buffers in ParsedCmd variants.

// Expects string: `"db_name")`
void pr_parse_create_db(ParsedCmd *cmd, char *query) {
  if (next_token(query, ')') == 0) {
    return;
  }
  // query now contains the db name
  trim_quotes(query);
  truncate_name(query);
  memcpy(cmd->cmd.create_db.db_name, query, MAX_SIZE_NAME);
  cmd->variant = PARSED_CREATE_DB;
}

// Expects string: `"table_name",db_name,num_cols)`
void pr_parse_create_tbl(ParsedCmd *cmd, char *query) {
  int32_t query_len = strlen(query);
  char *table_name = query;

  size_t db_offset = next_token(table_name, ',');
  if (db_offset == 0 || (query_len -= db_offset) <= 0) {
    return;
  }
  char *db_name = table_name + db_offset;

  size_t col_offset = next_token(db_name, ',');
  if (col_offset == 0 || (query_len -= col_offset) <= 0) {
    return;
  }
  char *num_cols = db_name + col_offset;

  trim_quotes(table_name);
  truncate_name(table_name);
  truncate_name(db_name);
  trim_parenthesis(num_cols);

  memcpy(cmd->cmd.create_table.table_name, table_name, MAX_SIZE_NAME);
  memcpy(cmd->cmd.create_table.db_name, db_name, MAX_SIZE_NAME);
  cmd->cmd.create_table.column_ct = atoi(num_cols);
  cmd->variant = PARSED_CREATE_TABLE;
}

// Expects string: `"project",awesomebase.grades)`
void pr_parse_create_col(ParsedCmd *cmd, char *query) {
  int32_t query_len = strlen(query);
  char *col_name = query;

  size_t db_offset = next_token(query, ',');
  if (db_offset == 0 || (query_len -= db_offset) <= 0) {
    return;
  }
  char *db_name = col_name + db_offset;

  size_t table_offset = next_token(db_name, '.');
  if (table_offset == 0 || (query_len -= table_offset) <= 0) {
    return;
  }
  char *table_name = db_name + table_offset;

  trim_quotes(col_name);
  truncate_name(col_name);
  trim_parenthesis(table_name);
  truncate_name(db_name);
  truncate_name(db_name);

  strncpy(cmd->cmd.create_col.col_name, col_name, MAX_SIZE_NAME - 1);
  cmd->cmd.create_col.col_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.create_col.db_name, db_name, MAX_SIZE_NAME - 1);
  cmd->cmd.create_col.db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.create_col.table_name, table_name, MAX_SIZE_NAME - 1);
  cmd->cmd.create_col.table_name[MAX_SIZE_NAME - 1] = '\0';

  cmd->variant = PARSED_CREATE_COL;
}

// Expects string: `awesomebase.grades,107,80,75,95 ...)`
void pr_parse_relational_insert(ParsedCmd *cmd, char *query) {
  int32_t query_len = strlen(query);

  char *db_name = query;
  size_t db_name_len = next_token(db_name, '.');
  if (db_name_len == 0 || (query_len -= db_name_len) <= 0) {
    return;
  }

  char *table_name = db_name + db_name_len;
  size_t table_name_len = next_token(table_name, ',');
  if (table_name_len == 0 || (query_len -= table_name_len) <= 0) {
    return;
  }

  char *val_cursor = table_name + table_name_len;
  size_t next_val_offset = 0;

  cmd->cmd.insert_into.values = vec_new(8);
  Vec *vals = &cmd->cmd.insert_into.values;

  while ((next_val_offset = next_token(val_cursor, ','))) {
    vec_push(vals, (Generic){.uint = atoi(val_cursor)});
    if ((query_len -= next_val_offset) <= 0) {
      // Last character should be a closing parenthesis, not just
      // a number.
      vec_free(vals);
      return;
    }
    val_cursor += next_val_offset;
  }

  // Last num doesn't have a comma after it, so we need to parse it
  // separately
  if (!next_token(val_cursor, ')')) {
    vec_free(vals);
    return;
  }
  vec_push(vals, (Generic){.uint = atoi(val_cursor)});

  strncpy(cmd->cmd.insert_into.db_name, db_name, MAX_SIZE_NAME - 1);
  cmd->cmd.insert_into.db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.insert_into.table_name, table_name, MAX_SIZE_NAME - 1);
  cmd->cmd.insert_into.table_name[MAX_SIZE_NAME - 1] = '\0';

  cmd->variant = PARSED_INSERT_INTO;
}

// single use result type
typedef struct MaybeParsePrintable {
  StatusCode status;
  ParsePrintable result;
} MaybeParsePrintable;

/// Helper to pr_parse_print. Expects a null-terminated string, and
/// returns either a chandle name or column path depending on the input.
///
/// The caller is responsible for null-terminating the column/chandle name
/// before calling this function.
MaybeParsePrintable pr_parse_print_variant(char *name) {
  if (name[0] == '\0') {
    return (MaybeParsePrintable){.status = ERROR};
  }

  char *table_name = strchr(name, '.');
  if (table_name == NULL) {
    ParsePrintable printable = (ParsePrintable){.type = PRINT_FETCHED};
    strncpy(printable.val.fetched.chandle, name, MAX_SIZE_NAME - 1);
    printable.val.fetched.chandle[MAX_SIZE_NAME - 1] = '\0';
    return (MaybeParsePrintable){.status = OK, .result = printable};
  }
  *table_name = '\0';
  table_name++;

  char *col_name = strchr(table_name, '.');
  if (col_name == NULL) {
    return (MaybeParsePrintable){.status = ERROR};
  }
  *col_name = '\0';
  col_name++;

  ParsePrintable printable = (ParsePrintable){.type = PRINT_COLUMN};

  ParsePrintColumn *res = &printable.val.column;
  strncpy(res->db_name, name, MAX_SIZE_NAME - 1);
  res->db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(res->table_name, table_name, MAX_SIZE_NAME - 1);
  res->table_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(res->col_name, col_name, MAX_SIZE_NAME - 1);
  res->col_name[MAX_SIZE_NAME - 1] = '\0';

  return (MaybeParsePrintable){.status = OK, .result = printable};
}

// Expects string: `awesomebase.grades.project,val_quizzes)`
void pr_parse_print(ParsedCmd *cmd, char *query) {
  Vec results = vec_new(8);
  char *next_break = NULL;
  while ((next_break = strpbrk(query, ",)"))) {
    *next_break = '\0';

    MaybeParsePrintable val = pr_parse_print_variant(query);
    if (val.status == ERROR) {
      for (size_t idx = 0; idx < results.len; idx++) {
        free(results.arr[idx].ptr);
      }
      vec_free(&results);
      return;
    }
    ParsePrintable *boxed = MALLOC(sizeof(ParsePrintable));
    *boxed = val.result;
    vec_push(&results, (Generic){.ptr = boxed});

    query = next_break + 1;
  }

  cmd->cmd.print_vals = (PrintVals){.printable = results};
  cmd->variant = PARSED_PRINT;
}

// Expects string: `awesomebase.grades.project,90,100)`
void pr_parse_select_col(ParsedCmd *cmd, char *query) {
  char *db_name = query;

  char *table_name = strchr(db_name, '.');
  if (table_name == NULL) {
    return;
  }
  *table_name = '\0';
  table_name++;

  char *column_name = strchr(table_name, '.');
  if (column_name == NULL) {
    return;
  }
  *column_name = '\0';
  column_name++;

  char *min_cond = strchr(column_name, ',');
  if (min_cond == NULL) {
    return;
  }
  *min_cond = '\0';
  min_cond++;

  char *max_cond = strchr(min_cond, ',');
  if (max_cond == NULL) {
    return;
  }
  *max_cond = '\0';
  max_cond++;
  trim_parenthesis(max_cond);

  int min_incl = INT_MIN;
  if (strcmp(min_cond, "null") != 0) {
    min_incl = atoi(min_cond);
  }
  if (min_incl == 0 && min_cond[0] != '0') {
    return;
  }

  int max_incl = INT_MAX;
  if (strcmp(max_cond, "null") != 0) {
    max_incl = atoi(max_cond) - 1;
  }
  if (max_incl == -1 && max_cond[0] != '0') {
    return;
  }

  cmd->cmd.select_col.min_incl = min_incl;
  cmd->cmd.select_col.max_incl = max_incl;

  strncpy(cmd->cmd.select_col.db_name, db_name, MAX_SIZE_NAME - 1);
  cmd->cmd.select_col.db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.select_col.table_name, table_name, MAX_SIZE_NAME - 1);
  cmd->cmd.select_col.table_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.select_col.col_name, column_name, MAX_SIZE_NAME - 1);
  cmd->cmd.select_col.col_name[MAX_SIZE_NAME - 1] = '\0';

  cmd->variant = PARSED_SELECT_COL;
}

// Expects string: `s1,sf1,-91098,225129)`
void pr_parse_select_chname(ParsedCmd *cmd, char *query) {
  char *bv_chname = query;

  char *iv_chname = strchr(query, ',');
  if (iv_chname == NULL) {
    log_err("Failed to parse chname, missing first comma: %s\n", query);
    return;
  }
  *iv_chname = '\0';
  iv_chname++;

  char *min_cond = strchr(iv_chname, ',');
  if (min_cond == NULL) {
    log_err("select chname missing comma after iv: %s\n", iv_chname);
    return;
  }
  *min_cond = '\0';
  min_cond++;

  char *max_cond = strchr(min_cond, ',');
  if (max_cond == NULL) {
    log_err("select chname missing comma after min cond: %s\n", min_cond);
    return;
  }
  *max_cond = '\0';
  max_cond++;
  trim_parenthesis(max_cond);

  int min_incl = INT_MIN;
  if (strcmp(min_cond, "null") != 0) {
    min_incl = atoi(min_cond);
  }
  if (min_incl == 0 && min_cond[0] != '0') {
    log_err("select chname failed to parse min as int: %s\n", min_cond);
    return;
  }

  int max_incl = INT_MAX;
  if (strcmp(max_cond, "null") != 0) {
    max_incl = atoi(max_cond) - 1;
  }
  if (max_incl == -1 && max_cond[0] != '0') {
    log_err("select chname failed to parse max as int: %s\n", max_cond);
    return;
  }

  SelectChname res = (SelectChname){.min_incl = min_incl, .max_incl = max_incl};

  strncpy(res.bitvec_filter_chname, bv_chname, MAX_SIZE_NAME - 1);
  res.bitvec_filter_chname[MAX_SIZE_NAME - 1] = '\0';

  strncpy(res.intvec_nums_chname, iv_chname, MAX_SIZE_NAME - 1);
  res.intvec_nums_chname[MAX_SIZE_NAME - 1] = '\0';

  cmd->cmd.select_chname = res;
  cmd->variant = PARSED_SELECT_CHNAME;
}

// Expects string: `awesomebase.grades.project,90,100)`
// or `awesomebase.grades.project,90,null)`
// or `s1,sf1,-91098,225129)`
void pr_parse_select(ParsedCmd *cmd, char *query) {
  char *third_comma = strnchr(query, ',', 3);
  if (third_comma == NULL) {
    pr_parse_select_col(cmd, query);
    return;
  }
  pr_parse_select_chname(cmd, query);
}

// Expects string: `awesomebase.grades.student_id,a_plus)`
void pr_parse_fetch(ParsedCmd *cmd, char *query) {
  char *db_name = query;

  char *table_name = strchr(db_name, '.');
  if (table_name == NULL) {
    return;
  }
  *table_name = '\0';
  table_name++;

  char *column_name = strchr(table_name, '.');
  if (column_name == NULL) {
    return;
  }
  *column_name = '\0';
  column_name++;

  char *chname = strchr(column_name, ',');
  if (chname == NULL) {
    return;
  }
  *chname = '\0';
  chname++;
  trim_parenthesis(chname);

  strncpy(cmd->cmd.fetch_int.db_name, db_name, MAX_SIZE_NAME - 1);
  cmd->cmd.fetch_int.db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.fetch_int.table_name, table_name, MAX_SIZE_NAME - 1);
  cmd->cmd.fetch_int.table_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.fetch_int.col_name, column_name, MAX_SIZE_NAME - 1);
  cmd->cmd.fetch_int.col_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(cmd->cmd.fetch_int.chname, chname, MAX_SIZE_NAME - 1);
  cmd->cmd.fetch_int.chname[MAX_SIZE_NAME - 1] = '\0';

  cmd->variant = PARSED_FETCH_INT;
}

void pr_free_partial_csv_parse(LoadCsv *loaded) {
  for (size_t idx = 0; idx < loaded->cols.len; idx++) {
    CsvColumn *col = loaded->cols.arr[idx].ptr;
    vec_free(&col->vals);
    free(col);
  }
  vec_free(&loaded->cols);
}

// Expects string: "d.t.c,d.t.c\n1,2\n1,2)"
void pr_parse_load(ParsedCmd *cmd, char *query) {
  LoadCsv res = (LoadCsv){.cols = vec_new(8)};
  // res.cols is Vec<&CsvColumn>

  // Parse the header line
  while (query[0] != '\0') {
    char *db_name = query;

    char *table_name = strchr(db_name, '.');
    if (table_name == NULL) {
      pr_free_partial_csv_parse(&res);
      return;
    }
    *table_name = '\0';
    table_name++;

    char *col_name = strchr(table_name, '.');
    if (col_name == NULL) {
      pr_free_partial_csv_parse(&res);
      return;
    }
    *col_name = '\0';
    col_name++;

    char *col_sep = strpbrk(col_name, ",\n");
    if (col_sep == NULL) {
      pr_free_partial_csv_parse(&res);
      return;
    }
    char col_sep_char = col_sep[0];
    *col_sep = '\0';

    CsvColumn *new_col = MALLOC(sizeof(CsvColumn));
    *new_col = (CsvColumn){.vals = vec_new(64)};

    strncpy(new_col->col_name, col_name, MAX_SIZE_NAME - 1);
    new_col->col_name[MAX_SIZE_NAME - 1] = '\0';

    vec_push(&res.cols, (Generic){.ptr = new_col});

    if (res.cols.len == 1) {
      strncpy(res.db_name, db_name, MAX_SIZE_NAME - 1);
      res.db_name[MAX_SIZE_NAME - 1] = '\0';

      strncpy(res.table_name, table_name, MAX_SIZE_NAME - 1);
      res.table_name[MAX_SIZE_NAME - 1] = '\0';
    } else {
      if (strcmp(res.db_name, db_name) != 0) {
        log_err("Inconsistent db_name across csv columns\n");
        pr_free_partial_csv_parse(&res);
        return;
      }
      if (strcmp(res.table_name, table_name) != 0) {
        log_err("Inconsistent table_name across csv columns\n");
        pr_free_partial_csv_parse(&res);
        return;
      }
    }

    query = col_sep + 1;
    if (col_sep_char == '\n') {
      break;
    }
  }

  // check duplicates
  // my inner leetcode is in shambles
  for (size_t slow_idx = 0; slow_idx < res.cols.len - 1; slow_idx++) {
    CsvColumn *slow_col = res.cols.arr[slow_idx].ptr;
    for (size_t fast_idx = slow_idx + 1; fast_idx < res.cols.len; fast_idx++) {
      CsvColumn *fast_col = res.cols.arr[fast_idx].ptr;
      if (strcmp(slow_col->col_name, fast_col->col_name) == 0) {
        log_err("Detected duplicate columns. Ensure csv cols are one to one "
                "with db cols\n");
        pr_free_partial_csv_parse(&res);
        return;
      }
    }
  }

  // Extract the csv nums
  while (query[0] != '\0' && query[0] != ')') {
    for (size_t src_idx = 0; src_idx < res.cols.len; src_idx++) {
      char *sep = strpbrk(query, ",\n)");
      if (sep == NULL) {
        log_err("Expected number for src idx %lu of %lu\n", src_idx,
                res.cols.len - 1);
        pr_free_partial_csv_parse(&res);
        return;
      }

      *sep = '\0';
      int val = atoi(query);
      CsvColumn *col = res.cols.arr[src_idx].ptr;
      vec_push(&col->vals, (Generic){.uint = val});
      query = sep + 1;
    }
  }

  cmd->cmd.load_csv = res;
  cmd->variant = PARSED_LOAD;
}

typedef struct ParseAggregateResult {
  StatusCode status;
  ParseAggregate val;
} ParseAggregateResult;

/// Accepts a string of either `db.tbl.col)` or `chandle)` and produces
/// a corresponding ParseAggregate value.
/// Modifies the string arbitrarily!
ParseAggregateResult pr_get_agg_target(char *query) {
  trim_parenthesis(query);
  char *table_name = strchr(query, '.');

  if (table_name == NULL) {
    // Must be a chandle
    ParseAggregateResult res = (ParseAggregateResult){
        .status = OK, .val = (ParseAggregate){.type = AGG_CHANDLE}};
    char *chname = res.val.val.chandle.chname;
    strncpy(chname, query, MAX_SIZE_NAME - 1);
    chname[MAX_SIZE_NAME - 1] = '\0';
    return res;
  }

  *table_name = '\0';
  table_name++;

  char *col_name = strchr(table_name, '.');
  if (col_name == NULL) {
    return (ParseAggregateResult){.status = ERROR};
  }
  *col_name = '\0';
  col_name++;

  ParseAggregateResult res = (ParseAggregateResult){
      .status = OK, .val = (ParseAggregate){.type = AGG_COLUMN}};
  ParseColumnAgg *col_path = &res.val.val.column;

  strncpy(col_path->db_name, query, MAX_SIZE_NAME - 1);
  col_path->db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(col_path->table_name, table_name, MAX_SIZE_NAME - 1);
  col_path->table_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(col_path->col_name, col_name, MAX_SIZE_NAME - 1);
  col_path->col_name[MAX_SIZE_NAME - 1] = '\0';

  return res;
}

// Expects string: "values1)"
void pr_parse_sum(ParsedCmd *cmd, char *query) {
  ParseAggregateResult parsed = pr_get_agg_target(query);
  if (parsed.status == ERROR) {
    return;
  }
  cmd->cmd.sum = parsed.val;
  cmd->variant = PARSED_SUM;
}

// Expects string: "values1)"
void pr_parse_avg(ParsedCmd *cmd, char *query) {
  ParseAggregateResult parsed = pr_get_agg_target(query);
  if (parsed.status == ERROR) {
    return;
  }
  cmd->cmd.sum = parsed.val;
  cmd->variant = PARSED_AVG;
}

// Expects string: "awesomebase.grades.midterm1,awesomebase.grades.midterm2)"
// or "chandle1,chandle2)"
void pr_parse_addsub(ParsedCmd *cmd, char *query, ParsedCmdEnum variant) {
  trim_parenthesis(query);
  char *right_side = strchr(query, ',');
  if (right_side == NULL) {
    return;
  }
  *right_side = '\0';
  right_side++;

  ParseAggregateResult left = pr_get_agg_target(query);
  if (left.status == ERROR) {
    return;
  }

  ParseAggregateResult right = pr_get_agg_target(right_side);
  if (right.status == ERROR) {
    return;
  }

  cmd->cmd.add = (ParseDualAggregate){.left = left.val, .right = right.val};
  cmd->variant = variant;
}

// Expects string: "values1)" or "db.tbl.col)"
void pr_parse_minmax(ParsedCmd *cmd, char *query, ParsedCmdEnum variant) {
  char *sep = strchr(query, ',');
  if (sep != NULL) {
    log_err("db doesn't yet support double min queries");
    return;
  }
  ParseAggregateResult parsed = pr_get_agg_target(query);
  if (parsed.status == ERROR) {
    return;
  }
  cmd->cmd.min_single = parsed.val;
  cmd->variant = variant;
}

void pr_free_partial_batch_select(BatchSelectCol *bsel) {
  Vec *chnames = &bsel->filters;
  for (size_t idx = 0; idx < chnames->len; idx++) {
    free((char *)chnames->arr[idx].ptr);
  }
  vec_free(chnames);
}

// Expects string: "DB.TBL.COL,s1,min1,max1,s2,min2,max2,...)"
void pr_parse_batch_select(ParsedCmd *cmd, char *query) {
  char *db = query;

  char *table = db + next_token(db, '.');
  if (db == table) {
    return;
  }

  char *col = table + next_token(table, '.');
  if (table == col) {
    return;
  }

  char *list = col + next_token(col, ',');
  if (col == list) {
    return;
  }

  BatchSelectCol bsel = (BatchSelectCol){.filters = vec_new(0)};
  while (*list != '\0') {
    char *chname = list;
    char *min = chname + next_token(chname, ',');
    char *max = min + next_token(min, ',');

    list = max + next_token(max, ',');
    if (list == max) {
      list = max + next_token(max, ')');
    }

    if (chname == min || min == max || max == list) {
      log_err(
          "next input didn't parse into a HANDLE,MIN,MAX triple: %s,%s,%s\n",
          chname, min, max);
      pr_free_partial_batch_select(&bsel);
      return;
    }

    SelectColFilter *filter = MALLOC(sizeof(SelectColFilter));

    // max - 1 converts exclusive to inclusive
    *filter =
        (SelectColFilter){.min_incl = atoi(min), .max_incl = atoi(max) - 1};
    if (strcmp(min, "null") == 0) {
      filter->min_incl = INT_MIN;
    }
    if (strcmp(max, "null") == 0) {
      filter->max_incl = INT_MAX;
    }
    strncpy(filter->handle, chname, MAX_SIZE_NAME - 1);
    filter->handle[MAX_SIZE_NAME - 1] = '\0';

    vec_push(&bsel.filters, (Generic){.ptr = filter});
  }

  strncpy(bsel.db_name, db, MAX_SIZE_NAME - 1);
  bsel.db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(bsel.table_name, table, MAX_SIZE_NAME - 1);
  bsel.table_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(bsel.col_name, col, MAX_SIZE_NAME - 1);
  bsel.col_name[MAX_SIZE_NAME - 1] = '\0';

  cmd->cmd.batch_select = bsel;
  cmd->variant = PARSED_BATCH_SELECT_COL;
}

// Expects string: "db.tbl.col,[btree, sorted],[clustered, unclustered])"
void pr_parse_create_idx(ParsedCmd *cmd, char *query) {
  char *db = query;

  char *table = db + next_token(db, '.');
  if (db == table) {
    return;
  }

  char *col = table + next_token(table, '.');
  if (table == col) {
    return;
  }

  char *config = col + next_token(col, ',');
  if (col == config) {
    return;
  }
  trim_parenthesis(config);
  char *variant_names[] = {"btree,clustered", "btree,unclustered",
                           "sorted,clustered", "sorted,unclustered"};
  IndexType variant_types[] = {IDX_CLUSTERED_BTREE, IDX_UNCLUSTERED_BTREE,
                               IDX_CLUSTERED_SORTED, IDX_UNCLUSTERED_SORTED};
  size_t len = sizeof(variant_types) / sizeof(IndexType);
  for (size_t idx = 0; idx < len; idx++) {
    if (strcmp(config, variant_names[idx]) == 0) {
      cmd->variant = PARSED_CREATE_IDX;
      CreateIndex *create = &cmd->cmd.create_idx;
      create->type = variant_types[idx];

      strncpy(create->db_name, db, MAX_SIZE_NAME - 1);
      create->db_name[MAX_SIZE_NAME - 1] = '\0';

      strncpy(create->table_name, table, MAX_SIZE_NAME - 1);
      create->table_name[MAX_SIZE_NAME - 1] = '\0';

      strncpy(create->col_name, col, MAX_SIZE_NAME - 1);
      create->col_name[MAX_SIZE_NAME - 1] = '\0';

      return;
    }
  }
  log_err("Failed to match create_idx variant: %s\n", config);
}

// Expects string: "f1,p1,f2,p2,nested-loop)" with cmd.handle == "t1,t2"
void pr_parse_join(ParsedCmd *cmd, char *query) {
  if (!cmd->handle || !query) {
    return;
  }

  char *out_chname1 = cmd->handle;
  char *out_chname2 = out_chname1 + next_token(out_chname1, ',');
  if (out_chname1 == out_chname2) {
    return;
  }

  char *fet_chname1 = query;
  char *sel_chname1 = fet_chname1 + next_token(fet_chname1, ',');
  char *fet_chname2 = sel_chname1 + next_token(sel_chname1, ',');
  char *sel_chname2 = fet_chname2 + next_token(fet_chname2, ',');
  char *jtype_str = sel_chname2 + next_token(sel_chname2, ',');

  if (fet_chname1 == sel_chname1 || sel_chname1 == fet_chname2 ||
      fet_chname2 == sel_chname2 || sel_chname2 == jtype_str || !jtype_str) {
    return;
  }

  trim_parenthesis(jtype_str);
  JoinType jtype = JOIN_GRACE_HASH;
  if (strcmp(jtype_str, "nested-loop") == 0) {
    jtype = JOIN_NESTED;
  } else if (strcmp(jtype_str, "naive-hash") == 0) {
    jtype = JOIN_SINGLE_HASH;
  } else if (strcmp(jtype_str, "hash") == 0) {
    jtype = JOIN_DECIDE_HASH;
  } else if (strcmp(jtype_str, "grace-hash") == 0) {
    jtype = JOIN_GRACE_HASH;
  } else {
    return;
  }

  ParseJoin *join = &cmd->cmd.join;
  join->jtype = jtype;
  copy_name(join->out_chname1, out_chname1);
  copy_name(join->out_chname2, out_chname2);
  copy_name(join->fet_chname1, fet_chname1);
  copy_name(join->sel_chname1, sel_chname1);
  copy_name(join->fet_chname2, fet_chname2);
  copy_name(join->sel_chname2, sel_chname2);

  cmd->variant = PARSED_JOIN;
}

// Expects string "db.tbl)"
void pr_parse_rebuild_indexes(ParsedCmd *cmd, char *query) {
  char *db = query;
  char *table = db + next_token(db, '.');
  if (db == table) {
    return;
  }
  if (next_token(table, ')') == 0) {
    return;
  }

  RebuildIndexes *req = &cmd->cmd.rebuild_indexes;

  strncpy(req->db_name, db, MAX_SIZE_NAME - 1);
  req->db_name[MAX_SIZE_NAME - 1] = '\0';

  strncpy(req->table_name, table, MAX_SIZE_NAME - 1);
  req->table_name[MAX_SIZE_NAME - 1] = '\0';
  cmd->variant = PARSED_REBUILD_INDEXES;
}

ParsedCmd parse_cmd(char *query) {
  ParsedCmd cmd = (ParsedCmd){.variant = PARSED_IGNORE,
                              .cmd.empty = NULL,
                              .handle = NULL,
                              .status = OK_DONE};
  if (strlen(query) < 2 || strncmp(query, "--", 2) == 0) {
    cmd.status = OK_DONE;
    return cmd;
  }
  trim_comment(query);

  // Extract the handle if one exists. Point query to whatever
  // is after the equal sign.
  char *points_to_eq = strchr(query, '=');
  if (points_to_eq != NULL) {
    *points_to_eq = '\0';
    cmd.handle = trim_whitespace(query);
    query = points_to_eq + 1;
  }

  // cs165_log(stdout, "QUERY: %s\n", query);

  cmd.status = OK_WAIT_FOR_RESPONSE;
  char *next_args = NULL;

  if ((next_args = first_char_after_match(query, "load("))) {
    // CSVs are sensitive to whitespace, we need newlines to remain
    pr_parse_load(&cmd, trim_spaces(next_args));
    cmd.status = OK_DONE;
    return cmd;
  }

  query = trim_whitespace(query);

  if ((next_args = first_char_after_match(query, "create(db,"))) {
    pr_parse_create_db(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "create(tbl,"))) {
    pr_parse_create_tbl(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "create(col,"))) {
    pr_parse_create_col(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "create(idx,"))) {
    pr_parse_create_idx(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args =
                  first_char_after_match(query, "relational_insert("))) {
    pr_parse_relational_insert(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "select("))) {
    pr_parse_select(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "fetch("))) {
    pr_parse_fetch(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "print("))) {
    pr_parse_print(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "shutdown"))) {
    cmd.variant = PARSED_SHUTDOWN;
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "sum("))) {
    pr_parse_sum(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "avg("))) {
    pr_parse_avg(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "add("))) {
    pr_parse_addsub(&cmd, next_args, PARSED_ADD);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "sub("))) {
    pr_parse_addsub(&cmd, next_args, PARSED_SUB);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "min("))) {
    pr_parse_minmax(&cmd, next_args, PARSED_MIN_SINGLE);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "max("))) {
    pr_parse_minmax(&cmd, next_args, PARSED_MAX_SINGLE);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "batch_select("))) {
    pr_parse_batch_select(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "single_core()"))) {
    cmd.cmd = (ParsedCmdUnion){.empty = NULL};
    cmd.variant = PARSED_SINGLE_THREAD_START;
    cmd.status = OK_DONE;
  } else if ((next_args =
                  first_char_after_match(query, "single_core_execute()"))) {
    cmd.cmd = (ParsedCmdUnion){.empty = NULL};
    cmd.variant = PARSED_SINGLE_THREAD_STOP;
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "rebuild_indexes("))) {
    pr_parse_rebuild_indexes(&cmd, next_args);
    cmd.status = OK_DONE;
  } else if ((next_args = first_char_after_match(query, "join("))) {
    pr_parse_join(&cmd, next_args);
    cmd.status = OK_DONE;
  } else {
    cmd.status = INCORRECT_FORMAT;
  }

  return cmd;
}

void parsed_cmd_free(ParsedCmd *cmd) {
  switch (cmd->variant) {
  case PARSED_IGNORE:
  case PARSED_CREATE_DB:
  case PARSED_CREATE_TABLE:
  case PARSED_CREATE_COL:
  case PARSED_SELECT_COL:
  case PARSED_SELECT_CHNAME:
  case PARSED_FETCH_INT:
  case PARSED_SHUTDOWN:
  case PARSED_SUM:
  case PARSED_AVG:
  case PARSED_ADD:
  case PARSED_SUB:
  case PARSED_MIN_SINGLE:
  case PARSED_MAX_SINGLE:
  case PARSED_SINGLE_THREAD_START:
  case PARSED_SINGLE_THREAD_STOP:
  case PARSED_CREATE_IDX:
  case PARSED_REBUILD_INDEXES:
  case PARSED_JOIN:
    break;
  case PARSED_INSERT_INTO: {
    vec_free(&cmd->cmd.insert_into.values);
    break;
  }
  case PARSED_PRINT: {
    Vec *vals = &cmd->cmd.print_vals.printable;
    for (size_t idx = 0; idx < vals->len; idx++) {
      free(vals->arr[idx].ptr);
    }
    vec_free(vals);
    break;
  }
  case PARSED_LOAD: {
    pr_free_partial_csv_parse(&cmd->cmd.load_csv);
    break;
  }
  case PARSED_BATCH_SELECT_COL: {
    pr_free_partial_batch_select(&cmd->cmd.batch_select);
    break;
  }
  }
  return;
}
