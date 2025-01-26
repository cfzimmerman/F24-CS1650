#ifndef CZ_OPERATORS
#define CZ_OPERATORS

#include "api2.h"
#include "bitvec.h"
#include "catalogue.h"
#include "client_context.h"
#include "parse2.h"
#include "threadpool.h"
#include "vector.h"

/// The number of characters allocated to handle a single integer. Overshoots,
/// but this is a nice round number.
#define MAX_CHARS_PER_INT 32

/// Use a a full table scan if selectivity exceeds this.
#define SMOOTH_SCAN_SEL 0.1

typedef struct {
  Status status;
  Chandle chandle;
} SelectResult;

typedef struct {
  Status status;
  ChandleIntVec results;
} FetchResult;

typedef struct {
  Status status;
  SaferStr str;
} PrintResult;

/// The output of a fallible aggregator operation.
/// If Status is ERROR, `val` will be UNINITIALIZED.
typedef struct {
  Status status;
  double val;
} AggRes;

/// A result type with a slice that can be aggregated.
typedef struct {
  Status status;
  size_t len;
  int *nums;
} AggNums;

typedef struct {
  BitVec result;
  int min_incl;
  int max_incl;
} BatchSelectQuery;

typedef struct {
  Status status;
  Column2 *col;
  BatchSelectQuery *queries;
} BatchSelectQueryResult;

/// BatchSelectQuery constructor.
BatchSelectQuery bsel_query_new(size_t column_len, int min_incl, int max_incl);

/// BatchSelectQuery destructor.
void bsel_query_free(BatchSelectQuery *query);

/// Inserts the entries in vals into the specified table.
/// Fails if the db or table don't exist or if the size of
/// vals doesn't match the number of columns in the DB.
Status db_insert_row(Catalogue *ctlg, char *db, char *table, Vec *vals);

/// Searches the specified column for entries matching the given critera.
/// Writes results into a bitvec, which is returned on success.
///
/// If SelectResult.Status = ERR, all other fields are UNDEFINED.
/// Only access the results and column if the status is Ok.
SelectResult db_select_col(Catalogue *ctlg, char *db, char *table, char *column,
                           int min_incl, int max_incl, ThreadPool *threads,
                           size_t thread_ct);

BatchSelectQueryResult db_select_batch(Catalogue *ctlg, char *db_name,
                                       char *tbl_name, char *col_name,
                                       Vec *filters /* Vec<SelectColFilter> */,
                                       ThreadPool *threads, size_t thread_ct);

/// Materializes results from a selected output into a vector of integers,
/// which is returned on success.
///
/// Fails if the specified resources couldn't be found or if the chandle
/// behind chname is not the output of a select operator.
///
/// If FetchResult.Status = ERR, all other fields are UNDEFINED.
/// Only access the results and column if the status is Ok.
FetchResult db_fetch(Catalogue *ctlg, ClientCxt2 *cxt, char *db, char *table,
                     char *column, char *chname);

/// Returns a stringified representation of the columns in `printable`, which
/// should be the output vec from parsing print. Only succeeds if all the
/// printed sequences have the same number of integer entries.
PrintResult db_print(Catalogue *ctlg, ClientCxt2 *cxt, Vec *printable);

/// Scans col. For each input query, sets  each value's corresponding bit in
/// in the ouput bitvec equal to whether that value is within the given
/// inclusive bounds. 1 = true, 0 = false.
void col_select_batch(Column2 *col, BatchSelectQuery *queries,
                      size_t num_queries, ThreadPool *threads,
                      size_t thread_ct);

/// Pushes the value onto the given column, increasing file size to accomodate
/// the new value if needed.
Status col_push(char *db_name, char *table_name, Column2 *column, int value);

/// Materializes the filter's selected indices from col into an output int
/// vec.
ChandleIntVec col_fetch_bitvec(Column2 *col, BitVec *filter);

/// Inserts the CsvColumn entries from cols into the given table. Returns
/// error if any of the specified resources don't exist.
/// IMPORTANT: This function assumes there are no duplicate column names in
/// cols. The parser currently handles this.
Status db_load(Catalogue *ctlg, char *db_name, char *table_name,
               Vec *cols /*Vec<&CsvColumn> */);

// /// Performs a sum aggregation operation on the specified chandle.
// /// The given chandle must be an IntVec.
AggRes db_sum(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req);

// /// Performs an average aggregation operation on the specified chandle.
// /// The given chandle must be an IntVec.
AggRes db_avg(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req);

/// Returns the sum of the given integer nums.
int64_t sum_agg(int *nums, size_t len);

/// Returns the average of the given integer nums.
/// Returns INT_MAX if len is zero because asking for that is just not cool.
double avg_agg(int *nums, size_t len);

/// Returns the minimum of the given integer nums.
int min_agg(int *nums, size_t len);

/// Returns the maximum of the given integer nums.
int max_agg(int *nums, size_t len);

/// Performs the underlying computation of `db_add` without any checks.
/// Panics if left and right are different lengths.
ChandleIntVec col_add(AggNums *left, AggNums *right);

/// Returns a new int vec with the left + right added componentwise.
FetchResult db_add(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *left,
                   ParseAggregate *right);

ChandleIntVec col_sub(AggNums *left, AggNums *right);

FetchResult db_sub(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *left,
                   ParseAggregate *right);

/// Returns the error-checked minimum aggregate over a single column. Distinct
/// from the double min defined in the spec.
AggRes db_min_single(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req);

/// Returns the error-checked maximum aggregate over a single column. Distinct
/// from the double max defined in the spec.
AggRes db_max_single(Catalogue *ctlg, ClientCxt2 *cxt, ParseAggregate *req);

/// Performs selection over an intvec. Assumes each entry in the intvec
/// corresponds to a 1 in the base filter. Returns a BitVec with 1s at a
/// subset of base_filter indices.
BitVec chname_select_bitvec(const BitVec *base_filter,
                            const ChandleIntVec *intvec, int min_incl,
                            int max_incl);

/// Calls intvec_select on the filter and vals chandle names given. Errors if
/// the chandles are not found or are of the wrong type.
SelectResult db_select_chname(ClientCxt2 *cxt, char *filter, char *intvec_vals,
                              int min_incl, int max_incl);

void tbl_sort_primary(Table2 *tbl, Column2 *primary);

/// Rebuilds the database Index structures for table db.table. Wrapper over
/// tbl_rebuild_indexes.
SaferStr db_rebuild_indexes(Catalogue *ctlg, char *db_name, char *table_name);

void tbl_rebuild_indexes(Table2 *table, bool re_sort);

void tbl_invalidate_indexes(Table2 *table);

/// Extracts the position list from ch or converts ch to a position list if
/// possible. If status is err, ch is not a position list.
Status chandle_try_poslist(Chandle *ch);

typedef struct {
  Status status;
  ChandlePosList pos1;
  ChandlePosList pos2;
} JoinResult;

/// Returns a join on the two given columns.
JoinResult db_join(ClientCxt2 *cxt, ParseJoin *req, ThreadPool *tpool);

/// Builds new column statistics. The input column must be
/// sorted with duplicates for proper results.
ColumnStats stats_new(int *sorted_col, size_t len);

/// Returns a all-zero stats instance.
ColumnStats stats_empty();

/// Based on stats, guesses how selective the query range is.
/// Returns a probability between 0. and 1.
float stats_selectivity(ColumnStats *stats, int min_incl, int max_incl);

#endif
