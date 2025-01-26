#ifndef CZ_API
#define CZ_API

#include "bptree.h"
#include "bptree2.h"
#include "vector.h"
#include <stddef.h>

#ifndef SOCK_PATH
#define SOCK_PATH "cs165_unix_socket"
#endif

// Names cannot exceed 64 characters.
#define MAX_SIZE_NAME 64
#define HANDLE_MAX_SIZE 64
#define MAX_SIZE_DB_PATH MAX_SIZE_NAME * 3 + 2

/// The number of chars to allocate for a stringified
/// integer. This is overkill, but it's nicely aligned
/// and extra safe.
#define MAX_SIZE_INT 32

typedef enum {
  IDX_NONE,
  IDX_CLUSTERED_BTREE,
  IDX_CLUSTERED_SORTED,
  IDX_UNCLUSTERED_BTREE,
  IDX_UNCLUSTERED_SORTED,
} IndexType;

#define STATS_LEN 100UL

typedef struct {
  int histogram[STATS_LEN];
} ColumnStats;

typedef struct {
  // Maps keys to indices in all the other columns
  int *keys;
  Generic *pos; // Generic.uint = size_t
  // How long the keys and pos arrays are.
  // May be smaller due to dedup.
  size_t len;
} SortedIdx;

typedef union {
  void *empty;      // None
  SortedIdx sorted; // UnclusteredSorted and ClusteredSorted
  BPtree clustered_tree;
  BPtree2 unclustered_tree;
} IndexUnion;

/// Defines what index (if any) a given column has.
/// Indices live entirely in-memory, so only the index type is
/// serialized. If a deserialized index is custered, assume the
/// columns are already sorted.
typedef struct {
  IndexType type;
  IndexUnion val;
  ColumnStats stats; // Only used by unclustered indexes
} Index;

typedef struct {
  char name[MAX_SIZE_NAME];
  int *data;
  size_t len; // Number of integers (bytes / 4)
  size_t cap; // Number of integers (bytes / 4)
  Index idx;
} Column2;

typedef struct {
  char name[MAX_SIZE_NAME];
  Vec columns; // Vec<Column2>
  // Column index structures aren't always rebuilt when they
  // become invalid. Assume invalid indexes hold no memory and
  // have value empty regardless of enum tag.
  bool indexes_valid;
} Table2;

typedef struct {
  char name[MAX_SIZE_NAME];
  Vec tables;
} Db2;

typedef enum {
  OK,
  ERROR,
} StatusCode;

// status declares an error code and associated message
typedef struct {
  StatusCode code;
  char *error_message;
} Status;

static inline Status status_err(char *error_message) {
  return (Status){.code = ERROR, .error_message = error_message};
}

static inline Status status_ok() {
  return (Status){.code = OK, .error_message = NULL};
}

// status of a client request.
typedef enum {
  EMPTY,
  OK_DONE,
  OK_WAIT_FOR_RESPONSE,
  INCORRECT_FORMAT,
} IoMessageStatus;

// message is a single packet of information sent between client/server.
// message_status: defines the status of the message.
// length: defines the length of the string message to be sent.
// payload: defines the payload of the message.
typedef struct {
  IoMessageStatus status;
  int length;
  char *payload;
} IoMessage;

#endif
