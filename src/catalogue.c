#include "include/catalogue.h"
#include "include/api2.h"
#include "include/json_parse.h"
#include "include/mem.h"
#include "include/operators.h"
#include "include/utils.h"
#include "include/vector.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define NEW_FILE_SIZE_BYTES 4096

char *ctlg_path_column(char *db_name, char *table_name, char *column_name) {
  size_t buf_size = MAX_SIZE_NAME * 4;
  char *path = MALLOC(buf_size);
  snprintf(path, buf_size, DB_PATH_PREFIX "%s/%s/%s.db", db_name, table_name,
           column_name);
  return path;
}

/// Returns the table with the given name in DB or NULL if it
/// wasn't found.
Table2 *ctlg_find_table(Db2 *db, char *table_name) {
  for (size_t idx = 0; idx < db->tables.len; idx++) {
    Table2 *table = db->tables.arr[idx].ptr;
    if (strcmp(table->name, table_name) == 0) {
      return table;
    }
  }
  return NULL;
}

Column2 *ctlg_find_column(Table2 *table, char *column_name) {
  for (size_t idx = 0; idx < table->columns.len; idx++) {
    Column2 *col = table->columns.arr[idx].ptr;
    if (strcmp(col->name, column_name) == 0) {
      return col;
    }
  }
  return NULL;
}

FoundColumn find_column(Catalogue *ctlg, char *db, char *table, char *column) {
  if (ctlg->db == NULL || strcmp(ctlg->db->name, db) != 0) {
    return (FoundColumn){.status =
                             status_err("Requested db didn't match current db"),
                         .col = NULL};
  }
  Table2 *tbl = ctlg_find_table(ctlg->db, table);
  if (tbl == NULL) {
    log_info("searching for: %s.%s.%s\n", db, table, column);
    return (FoundColumn){.status = status_err("Selected table doesn't exist"),
                         .col = NULL};
  }
  Column2 *col = ctlg_find_column(tbl, column);
  if (col == NULL) {
    return (FoundColumn){.status = status_err("Selected column doesn't exist"),
                         .col = NULL};
  }
  return (FoundColumn){.status = status_ok(), .col = col};
}

/// Calls mkdir on the given path with a standardized set of permissions.
/// Returns true on success. Also returns success if mkdir fails because the
/// dir/file already exists.
bool pr_guarantee_dir(char *path) {
  if (mkdir(path, S_IRWXU) < 0) {
    return errno == EEXIST;
  }
  return true;
}

Db2 *ctlg_db_new(Catalogue *ctlg, char *db_name) {
  if (strlen(db_name) >= MAX_SIZE_NAME) {
    log_err("All names must be less than %d characters\n", MAX_SIZE_NAME);
    return NULL;
  }
  if (ctlg->db != NULL) {
    log_err("Cannot create a second DB, only one at a time is "
            "supported right now.");
    return NULL;
  }

  if (!pr_guarantee_dir(DB_PATH_PREFIX)) {
    log_err("Failed to mkdir at %s\n", DB_PATH_PREFIX);
    return NULL;
  }

  size_t buf_size = MAX_SIZE_NAME * 2;
  char path_buf[buf_size];
  snprintf(path_buf, buf_size, DB_PATH_PREFIX "%s", db_name);
  if (!pr_guarantee_dir(path_buf)) {
    log_err("Failed to mkdir at %s\n", path_buf);
    return NULL;
  }

  Db2 *db = MALLOC(sizeof(Db2));
  db->tables = vec_new(4);

  strncpy(db->name, db_name, MAX_SIZE_NAME - 1);
  db->name[MAX_SIZE_NAME - 1] = '\0';

  ctlg->db = db;
  return db;
}

Table2 *ctlg_table_new(Catalogue *ctlg, char *db_name, char *table_name,
                       size_t num_cols) {
  if (strlen(db_name) >= MAX_SIZE_NAME || strlen(table_name) >= MAX_SIZE_NAME) {
    log_err("All names must be less than %d characters\n", MAX_SIZE_NAME);
    return NULL;
  }
  if (ctlg->db == NULL) {
    log_err("DB uninitialized, cannot create table\n");
    return NULL;
  }
  if (strcmp(ctlg->db->name, db_name) != 0) {
    log_err("Current DB is %s, cannot serve request for %s\n", ctlg->db->name,
            db_name);
    return NULL;
  }
  if (ctlg_find_table(ctlg->db, table_name) != NULL) {
    log_err("Table %s.%s already exists, skipping creation\n", db_name,
            table_name);
    return NULL;
  }

  size_t buf_size = MAX_SIZE_NAME * 3;
  char path_buf[buf_size];
  snprintf(path_buf, buf_size, DB_PATH_PREFIX "%s/%s", db_name, table_name);
  if (!pr_guarantee_dir(path_buf)) {
    log_err("Failed to mkdir at %s\n", path_buf);
    return NULL;
  }

  Db2 *db = ctlg->db;
  Table2 *tbl = MALLOC(sizeof(Table2));
  tbl->columns = vec_new(num_cols);

  strncpy(tbl->name, table_name, MAX_SIZE_NAME - 1);
  tbl->name[MAX_SIZE_NAME - 1] = '\0';
  tbl->indexes_valid = false;

  vec_push(&db->tables, (Generic){.ptr = tbl});
  return tbl;
}

/// Returns the length of a file.
long pr_get_file_len(int fd) {
  struct stat st;
  if (fstat(fd, &st) < 0) {
    log_err("Failed to retrieve file size\n");
    return -1;
  }
  return st.st_size;
}

typedef struct NonemptyFile {
  int fd;
  size_t capacity;
} NonemptyFile;

/// Creates a new file at the given path with read/write permissions.
/// OR opens one that already exists.
/// If the file is empty or too small, initializes it with some memory to work
/// with. Returns fd = -1 if any of this failed. Otherwise returns the file
/// descriptor and the capacity of the file. Note that the capacity might not be
/// full of useful data.
NonemptyFile pr_nonempty_file(char *filepath) {
  int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("open");
    log_err("Failed to create a new column file at %s\n", filepath);
    return (NonemptyFile){.fd = -1, .capacity = 0};
  }

  long flen = pr_get_file_len(fd);
  if (flen <= NEW_FILE_SIZE_BYTES) {
    flen = NEW_FILE_SIZE_BYTES;
    if (ftruncate(fd, NEW_FILE_SIZE_BYTES) < 0) {
      log_err("Failed to truncate column file to %lu bytes\n",
              NEW_FILE_SIZE_BYTES);
      return (NonemptyFile){.fd = -1, .capacity = 0};
    }
  }

  return (NonemptyFile){.fd = fd, .capacity = flen};
}

Column2 *ctlg_column_new(Catalogue *ctlg, char *db_name, char *table_name,
                         char *column_name, size_t column_len) {
  if (strlen(db_name) >= MAX_SIZE_NAME || strlen(table_name) >= MAX_SIZE_NAME ||
      strlen(column_name) > MAX_SIZE_NAME) {
    log_err("All names must be less than %d characters\n", MAX_SIZE_NAME);
    return NULL;
  }
  if (ctlg->db == NULL) {
    log_err("DB uninitialized, cannot create column\n");
    return NULL;
  }
  if (strcmp(ctlg->db->name, db_name) != 0) {
    log_err("Current DB is %s, cannot serve request for %s\n", ctlg->db->name,
            db_name);
    return NULL;
  }

  Table2 *table = ctlg_find_table(ctlg->db, table_name);
  if (table == NULL) {
    log_err("Cannot insert column, %s.%s does not exist\n", db_name,
            table_name);
    return NULL;
  }
  if (ctlg_find_column(table, column_name) != NULL) {
    log_err("Skipping creation, column %s.%s.%s already exists\n", db_name,
            table_name, column_name);
    return NULL;
  }

  tbl_invalidate_indexes(table);

  char *filepath = ctlg_path_column(db_name, table_name, column_name);
  NonemptyFile file = pr_nonempty_file(filepath);
  if (file.fd < 0) {
    log_err("Failed to create %s\n", filepath);
    free(filepath);
    return NULL;
  }

  int *data =
      mmap(NULL, file.capacity, PROT_READ | PROT_WRITE, MAP_SHARED, file.fd, 0);
  close(file.fd);

  if (data == MAP_FAILED) {
    log_err("Failed to mmap %s\n", filepath);
    perror("mmap");
    free(filepath);
    return NULL;
  }

  Column2 *col = MALLOC(sizeof(Column2));

  strncpy(col->name, column_name, MAX_SIZE_NAME - 1);
  col->name[MAX_SIZE_NAME - 1] = '\0';

  col->len = column_len;
  col->cap = file.capacity / sizeof(int);
  col->data = data;
  col->idx = (Index){.type = IDX_NONE, .val = {.empty = NULL}};
  vec_push(&table->columns, (Generic){.ptr = col});

  if (col->cap < col->len) {
    log_err("Column capacity is less than column length, which indicates "
            "corruption!\n");
    free(filepath);
    munmap(data, file.capacity);
    exit(1);
  }

  free(filepath);
  return col;
}

/// Declares an index type for the given column.
/// This invalidates the table's existing index structures.
Column2 *ctlg_index_new(Catalogue *ctlg, char *db_name, char *table_name,
                        char *column_name, IndexType type) {
  if (type == IDX_NONE || ctlg->db == NULL) {
    return NULL;
  }

  // Make sure there's only one clustered index per table
  Table2 *table = ctlg_find_table(ctlg->db, table_name);
  if (table == NULL) {
    log_err("table not found: %s\n", table_name);
    return NULL;
  }
  if (type == IDX_CLUSTERED_BTREE || type == IDX_CLUSTERED_SORTED) {
    for (size_t col_idx = 0; col_idx < table->columns.len; col_idx++) {
      Column2 *col = table->columns.arr[col_idx].ptr;
      if (col->idx.type == IDX_CLUSTERED_BTREE ||
          col->idx.type == IDX_CLUSTERED_SORTED) {
        log_err("attempted to create a clustered index on %s while a clustered "
                "index on %s already exists\n",
                column_name, col->name);
        return NULL;
      }
    }
  }

  FoundColumn maybe_col = find_column(ctlg, db_name, table_name, column_name);
  if (maybe_col.status.code == ERROR) {
    log_err("find_column failed: %s\n", maybe_col.status.error_message);
    return NULL;
  }
  if (maybe_col.col->idx.type != IDX_NONE) {
    log_err("column already has an index! %d\n", maybe_col.col->idx.type);
    return NULL;
  }

  tbl_invalidate_indexes(table);
  Column2 *col = maybe_col.col;
  col->idx = (Index){
      .type = type, .val = (IndexUnion){.empty = NULL}, .stats = stats_empty()};

  return col;
}

char *serialize_column(Column2 *col) {
  const size_t BUF_LEN = MAX_SIZE_NAME * 8; // arbitrarily oversized
  char *index;
  switch (col->idx.type) {
  case IDX_NONE: {
    index = "null";
    break;
  }
  case IDX_CLUSTERED_BTREE: {
    index = "\"ClusteredBtree\"";
    break;
  }
  case IDX_CLUSTERED_SORTED: {
    index = "\"ClusteredSorted\"";
    break;
  }
  case IDX_UNCLUSTERED_BTREE: {
    index = "\"UnclusteredBtree\"";
    break;
  }
  case IDX_UNCLUSTERED_SORTED: {
    index = "\"UnclusteredSorted\"";
    break;
  }
  }
  char *buf = MALLOC(BUF_LEN);
  snprintf(buf, BUF_LEN, "{\"name\": \"%s\", \"length\": %lu, \"index\": %s}",
           col->name, col->len, index);
  return buf;
}

char *serialize_table(Table2 *table) {
  size_t buf_len = 512;
  size_t buf_idx = 0;
  char *serial_cols = MALLOC(buf_len);
  serial_cols[0] = '\0';
  for (size_t col_idx = 0; col_idx < table->columns.len; col_idx++) {
    if (col_idx != 0) {
      serial_cols[buf_idx++] = ',';
    }
    char *col = serialize_column(table->columns.arr[col_idx].ptr);
    size_t col_len = strlen(col) + 1;
    if (buf_idx + col_len >= buf_len) {
      buf_len = max_unsig(buf_len * 2, buf_len + col_len + 64);
      serial_cols = REALLOC(serial_cols, buf_len);
    }
    memcpy(&serial_cols[buf_idx], col, col_len);
    buf_idx += (col_len - 1);
    free(col);
  }

  size_t result_len = (MAX_SIZE_NAME * 2) + buf_len;
  char *res = MALLOC(result_len);
  snprintf(res, result_len, "{\"name\": \"%s\", \"columns\": [%s]}",
           table->name, serial_cols);
  free(serial_cols);
  return res;
}

char *serialize_db(Db2 *db) {
  size_t buf_len = 512;
  size_t buf_idx = 0;
  char *serial_db = MALLOC(buf_len);
  serial_db[0] = '\0';

  for (size_t tbl_idx = 0; tbl_idx < db->tables.len; tbl_idx++) {
    if (tbl_idx != 0) {
      serial_db[buf_idx++] = ',';
    }
    char *tbl = serialize_table(db->tables.arr[tbl_idx].ptr);
    size_t table_len = strlen(tbl) + 1;
    if (buf_idx + table_len >= buf_len) {
      buf_len = max_unsig(buf_len * 2, buf_len + table_len + 64);
      serial_db = REALLOC(serial_db, buf_len);
    }
    memcpy(&serial_db[buf_idx], tbl, table_len);
    buf_idx += (table_len - 1);
    free(tbl);
  }

  size_t result_len = (MAX_SIZE_NAME * 2) + buf_len;
  char *res = MALLOC(result_len);
  snprintf(res, result_len, "{\"name\": \"%s\", \"tables\": [%s]}", db->name,
           serial_db);
  free(serial_db);
  return res;
}

char *pr_serialize_catalogue(Catalogue *ctlg) {
  if (ctlg->db == NULL) {
    char *empty = "{}";
    char *mem = MALLOC(strlen(empty) + 1);
    strcpy(mem, empty);
    return mem;
  }

  char *db = serialize_db(ctlg->db);

  size_t result_len = (MAX_SIZE_NAME * 2) + strlen(db);
  char *res = MALLOC(result_len);
  snprintf(res, result_len, "{\"db\": %s}", db);

  free(db);

  return res;
}

void pr_ctlg_free(Catalogue *ctlg) {
  if (ctlg->db == NULL) {
    return;
  }
  for (size_t tbl_idx = 0; tbl_idx < ctlg->db->tables.len; tbl_idx++) {
    Table2 *table = ctlg->db->tables.arr[tbl_idx].ptr;

    tbl_invalidate_indexes(table);
    for (size_t col_idx = 0; col_idx < table->columns.len; col_idx++) {
      Column2 *col = table->columns.arr[col_idx].ptr;
      if (col->data) {
        if (munmap(col->data, col->cap) < 0) {
          log_err("Failed to munmap %s\n", col->name);
        }
      }
      free(col);
    }
    vec_free(&table->columns);
    free(table);
  }
  vec_free(&ctlg->db->tables);
  free(ctlg->db);
  ctlg->db = NULL;
}

char *ctlg_dehydrate(Catalogue *ctlg) {
  char *serial_ctlg = pr_serialize_catalogue(ctlg);
  pr_ctlg_free(ctlg);
  return serial_ctlg;
}

Catalogue ctlg_rehydrate(char *serialized) {
  Catalogue ctlg = (Catalogue){.db = NULL};
  JsonVal parsed = json_parse(serialized);
  if (parsed.type == JSON_INVALID) {
    // log_err("Json parse completely failed\n");
    return ctlg;
  }

  JsonVal *db_obj = json_obj_get(&parsed, "db");
  if (db_obj == NULL) {
    // log_err("Expected a db obj\n");
    return ctlg;
  }

  JsonVal *db_name_json = json_obj_get(db_obj, "name");
  if (db_name_json == NULL || db_name_json->type != JSON_STR) {
    // log_err("Expected db name\n");
    return ctlg;
  }
  char *db_name = db_name_json->val.str;
  if (ctlg_db_new(&ctlg, db_name) == NULL) {
    log_err("Failed to create a new db: %s\n", db_name);
    return ctlg;
  }

  JsonVal *tables = json_obj_get(db_obj, "tables");
  if (tables == NULL || tables->type != JSON_ARR) {
    // log_err("Expected a db tables array\n");
    return ctlg;
  }
  for (size_t tbl_idx = 0; tbl_idx < tables->val.arr.len; tbl_idx++) {
    JsonVal *tbl_obj = tables->val.arr.arr[tbl_idx].ptr;
    JsonVal *tbl_name_json = json_obj_get(tbl_obj, "name");
    if (tbl_name_json == NULL || tbl_name_json->type != JSON_STR) {
      // log_err("Expected table name\n");
      continue;
    }
    char *tbl_name = tbl_name_json->val.str;

    // Num columns = 4 is arbitrary. My columns are dynamically sized.
    Table2 *table = ctlg_table_new(&ctlg, db_name, tbl_name, 4);
    if (table == NULL) {
      log_err("Failed to create a new table: %s\n", tbl_name);
      return ctlg;
    }

    JsonVal *cols = json_obj_get(tbl_obj, "columns");
    if (cols == NULL || cols->type != JSON_ARR) {
      // log_err("Expected column array\n");
      continue;
    }

    for (size_t col_idx = 0; col_idx < cols->val.arr.len; col_idx++) {
      JsonVal *col_obj = cols->val.arr.arr[col_idx].ptr;
      JsonVal *col_name_json = json_obj_get(col_obj, "name");
      if (col_name_json == NULL || col_name_json->type != JSON_STR) {
        // log_err("Expected column name\n");
        continue;
      }
      char *col_name = col_name_json->val.str;

      JsonVal *col_len = json_obj_get(col_obj, "length");
      if (col_len == NULL || col_len->type != JSON_NUM) {
        // log_err("Expected column len\n");
        continue;
      }

      if (ctlg_column_new(&ctlg, db_name, tbl_name, col_name,
                          (size_t)col_len->val.num) == NULL) {
        log_err("Failed to create column: %s\n", col_name);
        continue;
      }
      JsonVal *index = json_obj_get(col_obj, "index");
      if (index == NULL) {
        log_err("Column json didn't include an index field");
        continue;
      }
      if (index->type == JSON_NULL) {
        continue;
      }
      if (index->type != JSON_STR) {
        log_err("Index json was not a string: %d\n", index->type);
        continue;
      }
      char *index_variants[] = {"ClusteredBtree", "UnclusteredBtree",
                                "ClusteredSorted", "UnclusteredSorted"};
      IndexType index_types[] = {IDX_CLUSTERED_BTREE, IDX_UNCLUSTERED_BTREE,
                                 IDX_CLUSTERED_SORTED, IDX_UNCLUSTERED_SORTED};
      size_t len = sizeof(index_types) / sizeof(IndexType);
      for (size_t idx = 0; idx < len; idx++) {
        if (strcmp(index_variants[idx], index->val.str) != 0) {
          continue;
        }
        if (ctlg_index_new(&ctlg, db_name, tbl_name, col_name,
                           index_types[idx]) == NULL) {
          log_err("Failed to create index: %s.%s.%s idx type %d\n", db_name,
                  tbl_name, col_name, index_types[idx]);
        }
        break;
      }
    }
    tbl_rebuild_indexes(table, false);
  }

  json_free(&parsed);
  return ctlg;
}

char *ctlg_readf(const char *filepath) {
  FILE *file = fopen(filepath, "r");
  if (file == NULL) {
    return NULL;
  }

  size_t buf_len = 512;
  size_t buf_idx = 0;
  char *buf = MALLOC(buf_len);

  int data;
  while ((data = fgetc(file)) != EOF) {
    buf[buf_idx++] = (char)data;
    if (buf_idx == buf_len) {
      buf_len *= 2;
      buf = REALLOC(buf, buf_len);
    }
  }
  fclose(file);

  buf[buf_idx] = '\0';
  return buf;
}

bool ctlg_writef(const char *filepath, const char *serialized) {
  FILE *file = fopen(filepath, "w+");
  if (file == NULL) {
    perror("fopen");
    log_err("Failed to open canonical file: %s\n", filepath);
    return false;
  }

  if (fputs(serialized, file) == EOF) {
    perror("fputs");
    log_err("Failed to fputs: %s\n", serialized);
    return false;
  }
  fclose(file);

  return true;
}
