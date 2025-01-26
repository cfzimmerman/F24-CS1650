// #include "include/mem.h"
// #include "include/message.h"
#include "include/api2.h"
#include "include/parse2.h"
#include "include/utils.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

void test_strnchr() {
  char *str = "a\nb\nc\nd\ne\nf";
  assert(strcmp("\nb\nc\nd\ne\nf", strnchr(str, '\n', 1)) == 0);
  assert(strcmp("\nd\ne\nf", strnchr(str, '\n', 3)) == 0);
  assert(strcmp("\nf", strnchr(str, '\n', 5)) == 0);
  assert(strnchr(str, '\n', 6) == NULL);
}

void test_trim_comment() {
  char buf[256];
  snprintf(buf, 256, "this is --a comment");
  trim_comment(buf);
  assert(strcmp(buf, "this is ") == 0);
}

int main() {
  test_trim_comment();
  test_strnchr();

  char input[512];

  {
    // Empty input is ignored
    ParsedCmd cmd = parse_cmd("");
    assert(cmd.variant == PARSED_IGNORE);
    assert(cmd.handle == NULL);
    parsed_cmd_free(&cmd);
  }

  {
    // Nonsense input is ignored
    strcpy(input, "nonsense");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_IGNORE);
    assert(cmd.handle == NULL);
    parsed_cmd_free(&cmd);
  }

  {
    // Db creation is properly parsed
    strcpy(input, "create(db,\"awesomebase\")");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_CREATE_DB);
    assert(cmd.handle == NULL);
    assert(strcmp(cmd.cmd.create_db.db_name, "awesomebase") == 0);
    parsed_cmd_free(&cmd);
  }

  {
    // Table creation is properly parsed
    strcpy(input, "create(tbl,\"grades\",awesomebase,6)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_CREATE_TABLE);
    assert(cmd.handle == NULL);
    assert(strcmp(cmd.cmd.create_table.table_name, "grades") == 0);
    assert(strcmp(cmd.cmd.create_table.db_name, "awesomebase") == 0);
    assert(cmd.cmd.create_table.column_ct == 6);
    parsed_cmd_free(&cmd);
  }

  {
    // Malformed table creation is correctly ignored
    strcpy(input, "create(tbl,\"grades\",");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_IGNORE);
    assert(cmd.handle == NULL);
    parsed_cmd_free(&cmd);
  }

  {
    // Column creation is properly parsed
    strcpy(input, "create(col, \"project\", awesomebase.grades)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_CREATE_COL);
    assert(cmd.handle == NULL);
    assert(strcmp(cmd.cmd.create_col.col_name, "project") == 0);
    assert(strcmp(cmd.cmd.create_col.db_name, "awesomebase") == 0);
    assert(strcmp(cmd.cmd.create_col.table_name, "grades") == 0);
    parsed_cmd_free(&cmd);
  }

  {
    // Malformed column creation is correctly ignored
    strcpy(input, "create(col, \"project\"");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_IGNORE);
    assert(cmd.handle == NULL);
    parsed_cmd_free(&cmd);
  }

  {
    // Value insertion is properly parsed
    strcpy(input, "relational_insert(awesomebase.grades,107,80,75,95,93,1)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_INSERT_INTO);
    assert(cmd.handle == NULL);
    assert(strcmp(cmd.cmd.insert_into.db_name, "awesomebase") == 0);
    assert(strcmp(cmd.cmd.insert_into.table_name, "grades") == 0);

    uint64_t expected[] = {107, 80, 75, 95, 93, 1};
    assert(cmd.cmd.insert_into.values.len == 6);
    for (size_t idx = 0; idx < 6; idx++) {
      assert(expected[idx] == cmd.cmd.insert_into.values.arr[idx].uint);
    }
    parsed_cmd_free(&cmd);
  }

  {
    // Try some bad insertions
    strcpy(input, "relational_insert(awesomebase.grades,107,80,");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_IGNORE);
    parsed_cmd_free(&cmd);
  }

  {
    // Try some bad insertions (this one doesn't have a trailing comma)
    strcpy(input, "relational_insert(awesomebase.grades,107,80");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_IGNORE);
    parsed_cmd_free(&cmd);
  }

  {
    // select with two bounds
    strcpy(input, "select(awesomebase.grades.project,90,100)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_SELECT_COL);
    assert(strcmp(cmd.cmd.select_col.db_name, "awesomebase") == 0);
    assert(strcmp(cmd.cmd.select_col.table_name, "grades") == 0);
    assert(strcmp(cmd.cmd.select_col.col_name, "project") == 0);
    assert(cmd.cmd.select_col.min_incl == 90);
    assert(cmd.cmd.select_col.max_incl == 99);
    parsed_cmd_free(&cmd);
  }

  {
    // select with upper bound
    strcpy(input, "select(awesomebase.grades.project,null,100)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_SELECT_COL);
    assert(cmd.cmd.select_col.min_incl == INT_MIN);
    assert(cmd.cmd.select_col.max_incl == 99);
    parsed_cmd_free(&cmd);
  }

  {
    // select without bounds
    strcpy(input, "select(awesomebase.grades.project,null,null)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_SELECT_COL);
    assert(cmd.cmd.select_col.min_incl == INT_MIN);
    assert(cmd.cmd.select_col.max_incl == INT_MAX);
    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "pos_2=select(awesomebase.grades.project,null,null)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_SELECT_COL);
    assert(strcmp(cmd.handle, "pos_2") == 0);
    assert(cmd.cmd.select_col.min_incl == INT_MIN);
    assert(cmd.cmd.select_col.max_incl == INT_MAX);
    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "new_bitvec=select(sel_bitvec,fet_intvec,-99,99)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_SELECT_CHNAME);
    assert(strcmp(cmd.handle, "new_bitvec") == 0);
    assert(strcmp(cmd.cmd.select_chname.bitvec_filter_chname, "sel_bitvec") ==
           0);
    assert(strcmp(cmd.cmd.select_chname.intvec_nums_chname, "fet_intvec") == 0);
    assert(cmd.cmd.select_chname.min_incl == -99);
    assert(cmd.cmd.select_chname.max_incl == 98);
    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "top_ids=fetch(awesomebase.grades.student_id,a_plus)");
    ParsedCmd cmd = parse_cmd(input);
    assert(cmd.variant == PARSED_FETCH_INT);
    assert(strcmp(cmd.handle, "top_ids") == 0);
    assert(strcmp(cmd.cmd.fetch_int.db_name, "awesomebase") == 0);
    assert(strcmp(cmd.cmd.fetch_int.table_name, "grades") == 0);
    assert(strcmp(cmd.cmd.fetch_int.col_name, "student_id") == 0);
    assert(strcmp(cmd.cmd.fetch_int.chname, "a_plus") == 0);
    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input,
           "print(awesomebase.grades.project, awesomebase.grades.quizzes)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_PRINT);
    Vec *parsed = &cmd.cmd.print_vals.printable;
    assert(parsed->len == 2);

    ParsePrintable *col1 = parsed->arr[0].ptr;
    assert(col1->type == PRINT_COLUMN);
    assert(strcmp(col1->val.column.db_name, "awesomebase") == 0);
    assert(strcmp(col1->val.column.table_name, "grades") == 0);
    assert(strcmp(col1->val.column.col_name, "project") == 0);

    ParsePrintable *col2 = parsed->arr[1].ptr;
    assert(col2->type == PRINT_COLUMN);
    assert(strcmp(col2->val.column.db_name, "awesomebase") == 0);
    assert(strcmp(col2->val.column.table_name, "grades") == 0);
    assert(strcmp(col2->val.column.col_name, "quizzes") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input,
           "print(val_studentid, awesomebase.grades.checkin, val_project)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_PRINT);
    Vec *parsed = &cmd.cmd.print_vals.printable;
    assert(parsed->len == 3);

    ParsePrintable *fet1 = parsed->arr[0].ptr;
    assert(fet1->type == PRINT_FETCHED);
    assert(strcmp(fet1->val.fetched.chandle, "val_studentid") == 0);

    ParsePrintable *col = parsed->arr[1].ptr;
    assert(col->type == PRINT_COLUMN);
    assert(strcmp(col->val.column.db_name, "awesomebase") == 0);
    assert(strcmp(col->val.column.table_name, "grades") == 0);
    assert(strcmp(col->val.column.col_name, "checkin") == 0);

    ParsePrintable *fet2 = parsed->arr[2].ptr;
    assert(fet2->type == PRINT_FETCHED);
    assert(strcmp(fet2->val.fetched.chandle, "val_project") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    char *csv = "load(db.tbl.col1,db.tbl.col2,db.tbl.col3\n"
                "-45,84,65\n"
                "18,100,-72\n"
                "80,-57,86)";
    strcpy(input, csv);
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_LOAD);
    LoadCsv *metadata = &cmd.cmd.load_csv;

    assert(strcmp(metadata->db_name, "db") == 0);
    assert(strcmp(metadata->table_name, "tbl") == 0);
    assert(metadata->cols.len == 3);

    CsvColumn *col1 = metadata->cols.arr[0].ptr;
    assert(strcmp(col1->col_name, "col1") == 0);
    assert(col1->vals.len == 3);
    assert((int)col1->vals.arr[0].uint == -45);
    assert((int)col1->vals.arr[1].uint == 18);
    assert((int)col1->vals.arr[2].uint == 80);

    CsvColumn *col2 = metadata->cols.arr[1].ptr;
    assert(strcmp(col2->col_name, "col2") == 0);
    assert(col2->vals.len == 3);
    assert((int)col2->vals.arr[0].uint == 84);
    assert((int)col2->vals.arr[1].uint == 100);
    assert((int)col2->vals.arr[2].uint == -57);

    CsvColumn *col3 = metadata->cols.arr[2].ptr;
    assert(strcmp(col3->col_name, "col3") == 0);
    assert(col3->vals.len == 3);
    assert((int)col3->vals.arr[0].uint == 65);
    assert((int)col3->vals.arr[1].uint == -72);
    assert((int)col3->vals.arr[2].uint == 86);

    parsed_cmd_free(&cmd);
  }

  {
    // Identical to the test above except there's a newline after every input
    // row here. We don't know what to expect from the given csv.
    char *csv = "load(db.tbl.col1,db.tbl.col2,db.tbl.col3\n"
                "-45,84,65\n"
                "18,100,-72\n"
                "80,-57,86\n)";
    strcpy(input, csv);
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_LOAD);
    LoadCsv *metadata = &cmd.cmd.load_csv;

    assert(strcmp(metadata->db_name, "db") == 0);
    assert(strcmp(metadata->table_name, "tbl") == 0);
    assert(metadata->cols.len == 3);

    CsvColumn *col1 = metadata->cols.arr[0].ptr;
    assert(strcmp(col1->col_name, "col1") == 0);
    assert(col1->vals.len == 3);
    assert((int)col1->vals.arr[0].uint == -45);
    assert((int)col1->vals.arr[1].uint == 18);
    assert((int)col1->vals.arr[2].uint == 80);

    CsvColumn *col2 = metadata->cols.arr[1].ptr;
    assert(strcmp(col2->col_name, "col2") == 0);
    assert(col2->vals.len == 3);
    assert((int)col2->vals.arr[0].uint == 84);
    assert((int)col2->vals.arr[1].uint == 100);
    assert((int)col2->vals.arr[2].uint == -57);

    CsvColumn *col3 = metadata->cols.arr[2].ptr;
    assert(strcmp(col3->col_name, "col3") == 0);
    assert(col3->vals.len == 3);
    assert((int)col3->vals.arr[0].uint == 65);
    assert((int)col3->vals.arr[1].uint == -72);
    assert((int)col3->vals.arr[2].uint == 86);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "sum_quizzes=sum(values1)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_SUM);
    assert(cmd.cmd.sum.type == AGG_CHANDLE);
    assert(strcmp(cmd.cmd.sum.val.chandle.chname, "values1") == 0);
    assert(strcmp(cmd.handle, "sum_quizzes") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "sum_total=sum(db1.tbl1.col1)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_SUM);
    assert(cmd.cmd.sum.type == AGG_COLUMN);
    assert(strcmp(cmd.cmd.sum.val.column.db_name, "db1") == 0);
    assert(strcmp(cmd.cmd.sum.val.column.table_name, "tbl1") == 0);
    assert(strcmp(cmd.cmd.sum.val.column.col_name, "col1") == 0);
    assert(strcmp(cmd.handle, "sum_total") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "avg_quizzes=avg(values1)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_AVG);
    assert(cmd.cmd.avg.type == AGG_CHANDLE);
    assert(strcmp(cmd.cmd.avg.val.chandle.chname, "values1") == 0);
    assert(strcmp(cmd.handle, "avg_quizzes") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "midterms=add(awesomebase.grades.midterm1,awesomebase.grades."
                  "midterm2)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_ADD);
    assert(strcmp(cmd.handle, "midterms") == 0);

    assert(cmd.cmd.add.left.type == AGG_COLUMN);
    assert(strcmp(cmd.cmd.add.left.val.column.db_name, "awesomebase") == 0);
    assert(strcmp(cmd.cmd.add.left.val.column.table_name, "grades") == 0);
    assert(strcmp(cmd.cmd.add.left.val.column.col_name, "midterm1") == 0);

    assert(cmd.cmd.add.right.type == AGG_COLUMN);
    assert(strcmp(cmd.cmd.add.right.val.column.db_name, "awesomebase") == 0);
    assert(strcmp(cmd.cmd.add.right.val.column.table_name, "grades") == 0);
    assert(strcmp(cmd.cmd.add.right.val.column.col_name, "midterm2") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "a11=add(f11,f12)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_ADD);
    assert(strcmp(cmd.handle, "a11") == 0);

    assert(strcmp(cmd.cmd.add.left.val.chandle.chname, "f11") == 0);
    assert(strcmp(cmd.cmd.add.right.val.chandle.chname, "f12") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "min1=min(values1)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_MIN_SINGLE);
    assert(strcmp(cmd.handle, "min1") == 0);

    assert(cmd.cmd.min_single.type == AGG_CHANDLE);
    assert(strcmp(cmd.cmd.min_single.val.chandle.chname, "values1") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "max1=max(ddbb.tt.cc)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_MAX_SINGLE);
    assert(strcmp(cmd.handle, "max1") == 0);

    assert(cmd.cmd.min_single.type == AGG_COLUMN);
    assert(strcmp(cmd.cmd.max_single.val.column.db_name, "ddbb") == 0);
    assert(strcmp(cmd.cmd.max_single.val.column.table_name, "tt") == 0);
    assert(strcmp(cmd.cmd.max_single.val.column.col_name, "cc") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input,
           "batch_select(db1.tbl3_batch.col1,s1,10,20,s2,800,830,s3,50,500)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_BATCH_SELECT_COL);

    assert(strcmp(cmd.cmd.batch_select.db_name, "db1") == 0);
    assert(strcmp(cmd.cmd.batch_select.table_name, "tbl3_batch") == 0);
    assert(strcmp(cmd.cmd.batch_select.col_name, "col1") == 0);

    assert(cmd.cmd.batch_select.filters.len == 3);

    SelectColFilter *fil1 = cmd.cmd.batch_select.filters.arr[0].ptr;
    assert(strcmp(fil1->handle, "s1") == 0);
    assert(fil1->min_incl == 10);
    assert(fil1->max_incl == 19);

    SelectColFilter *fil2 = cmd.cmd.batch_select.filters.arr[1].ptr;
    assert(strcmp(fil2->handle, "s2") == 0);
    assert(fil2->min_incl == 800);
    assert(fil2->max_incl == 829);

    SelectColFilter *fil3 = cmd.cmd.batch_select.filters.arr[2].ptr;
    assert(strcmp(fil3->handle, "s3") == 0);
    assert(fil3->min_incl == 50);
    assert(fil3->max_incl == 499);

    parsed_cmd_free(&cmd);
  }

  {
    char *queries[] = {"create(idx, db.tbl.col, btree, clustered)",
                       "create(idx, db.tbl.col, btree, unclustered)",
                       "create(idx, db.tbl.col, sorted, clustered)",
                       "create(idx, db.tbl.col, sorted, unclustered)"};
    IndexType idx_types[] = {IDX_CLUSTERED_BTREE, IDX_UNCLUSTERED_BTREE,
                             IDX_CLUSTERED_SORTED, IDX_UNCLUSTERED_SORTED};
    size_t len = sizeof(idx_types) / sizeof(IndexType);
    for (size_t idx = 0; idx < len; idx++) {
      strcpy(input, queries[idx]);
      ParsedCmd cmd = parse_cmd(input);

      assert(cmd.variant == PARSED_CREATE_IDX);

      assert(cmd.cmd.create_idx.type == idx_types[idx]);
      assert(strcmp(cmd.cmd.create_idx.db_name, "db") == 0);
      assert(strcmp(cmd.cmd.create_idx.table_name, "tbl") == 0);
      assert(strcmp(cmd.cmd.create_idx.col_name, "col") == 0);

      parsed_cmd_free(&cmd);
    }
  }

  {
    strcpy(input, "rebuild_indexes(dbname.tblname)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_REBUILD_INDEXES);
    assert(strcmp(cmd.cmd.rebuild_indexes.db_name, "dbname") == 0);
    assert(strcmp(cmd.cmd.rebuild_indexes.table_name, "tblname") == 0);

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "r1, r2 = join(values1,positions1,values2,positions2,hash)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_JOIN);
    ParseJoin *join = &cmd.cmd.join;
    assert(join->jtype == JOIN_DECIDE_HASH);

    char *expected[] = {"r1",         "r2",      "values1",
                        "positions1", "values2", "positions2"};
    char *found[] = {join->out_chname1, join->out_chname2, join->fet_chname1,
                     join->sel_chname1, join->fet_chname2, join->sel_chname2};
    size_t len = sizeof(expected) / sizeof(char *);
    assert(len == sizeof(found) / sizeof(char *));

    for (size_t idx = 0; idx < len; idx++) {
      assert(strcmp(expected[idx], found[idx]) == 0);
    }

    parsed_cmd_free(&cmd);
  }

  {
    strcpy(input, "t1, t2 = join(f1, p1, f2, p2, grace-hash)");
    ParsedCmd cmd = parse_cmd(input);

    assert(cmd.variant == PARSED_JOIN);
    ParseJoin *join = &cmd.cmd.join;
    assert(join->jtype == JOIN_GRACE_HASH);

    char *expected[] = {"t1", "t2", "f1", "p1", "f2", "p2"};
    char *found[] = {join->out_chname1, join->out_chname2, join->fet_chname1,
                     join->sel_chname1, join->fet_chname2, join->sel_chname2};
    size_t len = sizeof(expected) / sizeof(char *);
    assert(len == sizeof(found) / sizeof(char *));

    for (size_t idx = 0; idx < len; idx++) {
      assert(strcmp(expected[idx], found[idx]) == 0);
    }

    parsed_cmd_free(&cmd);
  }

  printf("✅ %s\n", __FILE__);
  return 0;
}
