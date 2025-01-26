#include "include/api2.h"
#include "include/bptree.h"
#include "include/bptree2.h"
#include "include/catalogue.h"
#include "include/vector.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_empty() {
  Catalogue ctlg = (Catalogue){.db = NULL};
  char *serialized = ctlg_dehydrate(&ctlg);
  assert(strcmp(serialized, "{}") == 0);
  free(serialized);
}

void test_ctlg_test_db() {
  Catalogue ctlg = (Catalogue){.db = NULL};

  char *db_name = "ctlg_test_db";
  Db2 *db = ctlg_db_new(&ctlg, db_name);
  assert(db);

  char *table_name = "ctlg_test_table";
  Table2 *table = ctlg_table_new(&ctlg, db_name, table_name, 4);
  assert(table);

  assert(ctlg_column_new(&ctlg, db_name, table_name, "ctlg_test_col1", 0));
  assert(ctlg_column_new(&ctlg, db_name, table_name, "ctlg_test_col2", 0));
  assert(ctlg_column_new(&ctlg, db_name, table_name, "ctlg_test_col3", 0));
  assert(ctlg_column_new(&ctlg, db_name, table_name, "ctlg_test_col4", 0));

  char *serialized = ctlg_dehydrate(&ctlg);
  char *filepath = DB_PATH_PREFIX "ctlg_test_config.json";

  assert(ctlg_writef(filepath, serialized));
  char *read_back = ctlg_readf(filepath);

  assert(strcmp(read_back, serialized) == 0);

  Catalogue rehydrated = ctlg_rehydrate(read_back);
  assert(rehydrated.db != NULL);
  assert(rehydrated.db->tables.len == 1);
  assert(((Column2 *)rehydrated.db->tables.arr[0].ptr)->len == 4);

  char *re_serialized = ctlg_dehydrate(&rehydrated);
  assert(strcmp(re_serialized, serialized) == 0);

  free(read_back);
  free(serialized);
  free(re_serialized);
}

void test_serialize_column() {
  Column2 column = {.data = NULL,
                    .len = 99,
                    .cap = 1000,
                    .idx = {.type = IDX_CLUSTERED_BTREE,
                            .val = {.clustered_tree = bptree_new()}}};
  strcpy(column.name, "test_column");

  {
    const char *EXPECTED = "{\"name\": \"test_column\", \"length\": 99, "
                           "\"index\": \"ClusteredBtree\"}";
    char *col_ser = serialize_column(&column);

    assert(strcmp(col_ser, EXPECTED) == 0);
    free(col_ser);
  }

  {
    column.idx.type = IDX_NONE;
    bptree_free(&column.idx.val.clustered_tree);
    column.idx.val = (IndexUnion){.empty = NULL};
    const char *EXPECTED = "{\"name\": \"test_column\", \"length\": 99, "
                           "\"index\": null}";

    char *col_ser = serialize_column(&column);
    assert(strcmp(col_ser, EXPECTED) == 0);
    free(col_ser);
  }
}

void test_serialize_table() {
  Column2 column = {.data = NULL,
                    .len = 99,
                    .cap = 1000,
                    .idx = {.type = IDX_UNCLUSTERED_BTREE,
                            .val = {.unclustered_tree = bptree2_new()}}};
  strcpy(column.name, "test_column");

  Table2 table = {.columns = vec_new(4)};
  vec_push(&table.columns, (Generic){.ptr = &column});
  strcpy(table.name, "test_table");
  const char *EXPECTED = "{\"name\": \"test_table\", \"columns\": [{\"name\": "
                         "\"test_column\", \"length\": 99, "
                         "\"index\": \"UnclusteredBtree\"}]}";

  char *table_ser = serialize_table(&table);
  assert(strcmp(table_ser, EXPECTED) == 0);

  bptree2_free(&column.idx.val.unclustered_tree);
  vec_free(&table.columns);
  free(table_ser);
}

void test_serialize_db() {
  Column2 column = {
      .data = NULL,
      .len = 99,
      .cap = 1000,
      .idx = {.type = IDX_CLUSTERED_SORTED, .val = {.empty = NULL}}};
  strcpy(column.name, "test_column");

  Table2 table = {.columns = vec_new(4)};
  vec_push(&table.columns, (Generic){.ptr = &column});
  strcpy(table.name, "test_table");

  Db2 db = {.tables = vec_new(4)};
  vec_push(&db.tables, (Generic){.ptr = &table});
  strcpy(db.name, "test_database");

  char *EXPECTED = "{\"name\": \"test_database\", \"tables\": [{\"name\": "
                   "\"test_table\", \"columns\": [{\"name\": \"test_column\", "
                   "\"length\": 99, \"index\": \"ClusteredSorted\"}]}]}";
  char *db_ser = serialize_db(&db);
  assert(strcmp(db_ser, EXPECTED) == 0);

  vec_free(&table.columns);
  vec_free(&db.tables);
  free(db_ser);
}

int main() {
  test_empty();
  test_ctlg_test_db();

  test_serialize_column();
  test_serialize_table();
  test_serialize_db();

  printf("✅ %s\n", __FILE__);
  return 0;
}
