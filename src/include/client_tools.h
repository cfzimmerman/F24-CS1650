
#ifndef CZ_CLIENT_TOOLS
#define CZ_CLIENT_TOOLS

#include "api2.h"
#include "vector.h"
#include <stdbool.h>

/// If the given input is a load command, preprocesses
/// the CSV input and puts server-ready commands into the request buf.
bool preprocess_load(char *input, Vec *req_buf);

/// Used to accumulate a batched selection query.
/// If cmd_len == 0, assume no batched selection is
/// in progress. Any in-progress selection contains
/// at least the string `batch_select(`.
typedef struct BatchSelectAcc {
  /// The actual command being built up to send to the server.
  /// NEVER ASSUME THIS IS NULL TERMINATED. Use cmd_len instead.
  char *cmd;

  /// The number of initialized characters in cmd.
  size_t cmd_len;

  /// The number of characters cmd is capable of holding.
  size_t cmd_cap;

  /// If has_path == false, the value of `path` is meaningless.
  bool has_path;

  /// This name is null terminated.
  char path[MAX_SIZE_DB_PATH];
} BatchSelectAcc;

/// Creates a new default batch select accumulator.
BatchSelectAcc bsel_new();

/// Cleans up any resources held by the batch select accumulator.
void bsel_free(BatchSelectAcc *acc);

/// If the given input is related to batched select statements, updates
/// the accumulator or request buf accordingly.
/// Returns true if the input was related to batch selection and shouldn't be
/// sent directly to the server. If false, proceed as if this function did
/// nothing.
bool preprocess_batch_select(char *input, BatchSelectAcc *acc, Vec *req_buf);

/// Helper to preprocess load. Only public for easier testing.
char *make_rebuild_indexes_cmd(const Vec *req_buf);

#endif
