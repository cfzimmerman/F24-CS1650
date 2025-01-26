#ifndef CZ_PARSE
#define CZ_PARSE

#include "api2.h"
#include "client_context.h"
#include "vector.h"
#include <stdint.h>

typedef struct {
  Db2 *db;
  int client_socket;
  ClientCxt2 *client_cxt;
  Table2 *table;
} CmdContext;

typedef struct {
  char db_name[MAX_SIZE_NAME];
} CreateDb;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  size_t column_ct;
} CreateTable;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
} CreateCol;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  Vec values;
} InsertInto;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
  int min_incl;
  int max_incl;
} SelectCol;

typedef struct {
  char bitvec_filter_chname[MAX_SIZE_NAME];
  char intvec_nums_chname[MAX_SIZE_NAME];
  int min_incl;
  int max_incl;
} SelectChname;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
  char chname[MAX_SIZE_NAME];
} FetchInt;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
} ParsePrintColumn;

typedef struct {
  char chandle[MAX_SIZE_NAME];
} ParsePrintFetched;

typedef enum {
  PRINT_COLUMN,
  PRINT_FETCHED // Includes aggregates
} ParsePrintableTypes;

typedef struct {
  ParsePrintColumn column;
  ParsePrintFetched fetched;
} ParsePrintableUnion;

typedef struct {
  ParsePrintableTypes type;
  ParsePrintableUnion val;
} ParsePrintable;

typedef struct {
  Vec printable; // Vec<ParsePrintable>
} PrintVals;

typedef struct {
  char col_name[MAX_SIZE_NAME];
  Vec vals; // Vec<i32>
} CsvColumn;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  Vec cols; // Vec<CsvColumn>
} LoadCsv;

typedef struct {
  char chname[MAX_SIZE_NAME];
} ParseChandleAgg;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
} ParseColumnAgg;

typedef enum { AGG_CHANDLE, AGG_COLUMN } ParseAggType;

typedef union {
  ParseChandleAgg chandle;
  ParseColumnAgg column;
} ParseAggUnion;

typedef struct {
  ParseAggType type;
  ParseAggUnion val;
} ParseAggregate;

typedef struct {
  ParseAggregate left;
  ParseAggregate right;
} ParseDualAggregate;

typedef struct {
  char handle[MAX_SIZE_NAME];
  int min_incl;
  int max_incl;
} SelectColFilter;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
  // The operator in server.c depends on this Vec being SelectColFilter. Check
  // there if it changes.
  Vec filters; // Vec<SelectColFilter>
} BatchSelectCol;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
  char col_name[MAX_SIZE_NAME];
  IndexType type;
} CreateIndex;

typedef struct {
  char db_name[MAX_SIZE_NAME];
  char table_name[MAX_SIZE_NAME];
} RebuildIndexes;

typedef enum {
  JOIN_SINGLE_HASH,
  JOIN_GRACE_HASH,
  JOIN_NESTED,
  JOIN_DECIDE_HASH
} JoinType;
// t1,t2=join(f1,p1,f2,p2,nested-loop)
// r1, r2 = join(values1,positions1,values2,positions2,hash)
//
// positions1=select(awesomebase.cs165.project_score,100,null) -- select
// positions where project score >= 100 in cs165
// positions2=select(awesomebase.cs265.project_score,100,null) -- select
// positions where project score >= 100 in cs265
// values1=fetch(awesomebase.cs165.student_id,positions1)
// values2=fetch(awesomebase.cs265.student_id,positions2)
typedef struct {
  // out_chname1, out_chname2 =
  // join(fet_chname1,sel_chname1,fet_chname2,sel_chname2,jtype)
  char out_chname1[MAX_SIZE_NAME];
  char out_chname2[MAX_SIZE_NAME];
  char fet_chname1[MAX_SIZE_NAME];
  char sel_chname1[MAX_SIZE_NAME];
  char fet_chname2[MAX_SIZE_NAME];
  char sel_chname2[MAX_SIZE_NAME];
  JoinType jtype;
} ParseJoin;

typedef enum ParsedCmdEnum {
  PARSED_IGNORE,              // empty
  PARSED_CREATE_DB,           // create_db
  PARSED_CREATE_TABLE,        // create_table
  PARSED_CREATE_COL,          // create_col
  PARSED_INSERT_INTO,         // insert_into
  PARSED_SELECT_COL,          // select_col
  PARSED_SELECT_CHNAME,       // select_chname
  PARSED_FETCH_INT,           // fetch_int
  PARSED_PRINT,               // print_vals
  PARSED_LOAD,                // load_csv
  PARSED_SHUTDOWN,            // empty
  PARSED_SUM,                 // sum
  PARSED_AVG,                 // avg
  PARSED_ADD,                 // add
  PARSED_SUB,                 // sub
  PARSED_MIN_SINGLE,          // min_single
  PARSED_MAX_SINGLE,          // max_single
  PARSED_BATCH_SELECT_COL,    // batch_select
  PARSED_SINGLE_THREAD_START, // empty
  PARSED_SINGLE_THREAD_STOP,  // empty
  PARSED_CREATE_IDX,          // create_index
  PARSED_REBUILD_INDEXES,     // rebuild_indexes
  PARSED_JOIN,                // join
} ParsedCmdEnum;

/// Union with all the possible outputs a command may parse to.
typedef union ParsedCmdUnion {
  void *empty;
  CreateDb create_db;
  CreateTable create_table;
  CreateCol create_col;
  CreateIndex create_idx;
  InsertInto insert_into;
  SelectCol select_col;
  SelectChname select_chname;
  FetchInt fetch_int;
  PrintVals print_vals;
  LoadCsv load_csv;
  ParseAggregate sum;
  ParseAggregate avg;
  ParseDualAggregate add;
  ParseDualAggregate sub;
  ParseAggregate min_single;
  ParseAggregate max_single;
  BatchSelectCol batch_select;
  RebuildIndexes rebuild_indexes;
  ParseJoin join;
} ParsedCmdUnion;

typedef struct ParsedCmd {
  /// Identifies which union field to access.
  ParsedCmdEnum variant;
  /// Enumerates possible parse outputs.
  ParsedCmdUnion cmd;
  /// An id of the form `handle=cmd(...)` to associate results with.
  char *handle;
  /// A status to send the client based on what was parsed.
  IoMessageStatus status;
} ParsedCmd;

/// Returns a ParsedCommand enum given the input query command string.
/// Assume parse_command arbitrarily mutates the string. However, it's
/// still the caller's responsibility to free the string.
ParsedCmd parse_cmd(char *query_command);

/// Frees any memory held by the parsed command.
void parsed_cmd_free(ParsedCmd *cmd);

/// Finds the first instance of delim in query and sets it to \0.
/// Returns the number of bytes to jump from the start of query to
/// reach the first byte after the new null terminator (check for OOB!).
///
/// Returns 0 if there was no match.
size_t next_token(char *query, char delim);

#endif
