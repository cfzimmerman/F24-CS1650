#ifndef CZ_CATALOGUE
#define CZ_CATALOGUE

#include "api2.h"
#include <stdbool.h>

#define DB_PATH_PREFIX "../db/"

typedef struct Catalogue {
  // NULL if a db hasn't been created yet
  Db2 *db;
} Catalogue;

/// Retrieves the "catalogue.json" file from the given location, returning
/// its contents as a string.
/// Returns NULL if any file actions fail or if the catalogue doesn't exist.
///
/// Use path `DB_PATH_PREFIX "catalogue.json"` for the canonical location.
char *ctlg_readf(const char *filepath);

/// Writes a serialized catalogue to its standard location.
/// Returns false on failure.
bool ctlg_writef(const char *filepath, const char *serialized);

/// Parses a string into a Catalogue, setting up resources as needed.
Catalogue ctlg_rehydrate(char *serialized);

/// Serializes the catalogue into a JSON string. Releases all resources
/// held by the catalogue, leaving only the heap allocated string
/// returned to the caller.
/// The given Catalogue object itself is not freed, so the owner will
/// still have to free that if it's on the heap.
/// Write outputs into the catalogue file for persistence.
char *ctlg_dehydrate(Catalogue *ctlg);

Db2 *ctlg_db_new(Catalogue *ctlg, char *db_name);

Table2 *ctlg_table_new(Catalogue *ctlg, char *db_name, char *table_name,
                       size_t num_cols);

/// Creates a new column in this db and table. `column_len` is the length of
/// the existing column in INTEGERS (bytes / 4). Pass 0 if this is a
/// new column that doesn't have data yet.
Column2 *ctlg_column_new(Catalogue *ctlg, char *db_name, char *table_name,
                         char *column_name, size_t column_len);

/// Adds a new index to the given column. Returns NULL on failure. If the column
/// is empty, the index is uninitialized. If full, the index is loaded and ready
/// to use.
Column2 *ctlg_index_new(Catalogue *ctlg, char *db_name, char *table_name,
                        char *column_name, IndexType type);

Table2 *ctlg_find_table(Db2 *db, char *table_name);

Column2 *ctlg_find_column(Table2 *table, char *column_name);

char *ctlg_path_column(char *db_name, char *table_name, char *column_name);

typedef struct {
  Status status;
  Column2 *col;
} FoundColumn;

/// User-friendly wrapper over fetching db->table->column.
FoundColumn find_column(Catalogue *ctlg, char *db, char *table, char *column);

// These are public mostly for testing
char *serialize_column(Column2 *col);
char *serialize_table(Table2 *table);
char *serialize_db(Db2 *db);

#endif
