#ifndef CLIENT_CONTEXT_H
#define CLIENT_CONTEXT_H

#include "api2.h"
#include "bitvec.h"
#include "vector.h"

// CH stands for column handle.
// Chandle -> client handle.
// Chname -> client handle name.
// But calling it a chandle is just way more fun.

// Possible output from the select operator
typedef struct {
  BitVec bitvec;
  Column2 *column;
} ChandleBitVec;

// Possible output from the select operator
typedef struct {
  size_t start_idx;
  size_t len;
} ChandleSlice;

typedef struct {
  // List of column indices. uint64_t and not size_t
  // for memcpy compat with Generic
  uint64_t *pos;
  size_t len;
} ChandlePosList;

typedef struct {
  int *nums;
  size_t len;
} ChandleIntVec;

typedef enum { AGG_INTEGER, AGG_REAL } AggPrecision;

typedef struct {
  double val;
  AggPrecision prec;
} ChandleAggregate;

typedef enum {
  CHANDLE_BITVEC,
  CHANDLE_INTVEC,
  CHANDLE_AGGREGATE,
  CHANDLE_SLICE,
  CHANDLE_POSLIST,
} ChandleType;

typedef union {
  ChandleBitVec bv;
  ChandleIntVec iv;
  ChandleAggregate agg;
  ChandleSlice slice;
  ChandlePosList pos;
} ChandleUnion;

typedef struct {
  char chname[MAX_SIZE_NAME];
  ChandleType type;
  ChandleUnion val;
} Chandle;

typedef struct {
  Vec handles; // Vec<&Chandle>
} ClientCxt2;

Chandle *chandle_get(ClientCxt2 *cxt, char *chname);

/// Sets a chandle with name chname into the variable pool.
/// Intended for use with db operators that return unnamed Chandles.
Chandle *chandle_set(ClientCxt2 *cxt, char *chname, Chandle partial_chandle);
Chandle *chandle_set_bitvec(ClientCxt2 *cxt, char *chname, Column2 *column,
                            BitVec bitvec);
Chandle *chandle_set_intvec(ClientCxt2 *cxt, char *chname, int *nums,
                            size_t len);
Chandle *chandle_set_aggregate(ClientCxt2 *cxt, char *chname, double val,
                               AggPrecision precision);

void chandle_del(ClientCxt2 *cxt, char *chname);
void chandle_free(Chandle *chandle);

void cxt_free(ClientCxt2 *cxt);

#endif
