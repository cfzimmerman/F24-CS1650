#include "include/client_tools.h"
#include "include/api2.h"
#include "include/mem.h"
#include "include/parse2.h"
#include "include/utils.h"
#include "include/vector.h"
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

char *make_rebuild_indexes_cmd(const Vec *req_buf) {
  if (req_buf->len == 0) {
    return NULL;
  }
  // Expects command: "load(db.tbl.col1, ..."
  char *load_cmd = req_buf->arr[req_buf->len - 1].ptr;
  char *db_start = strchr(load_cmd, '(');
  if (db_start == NULL) {
    return NULL;
  }
  db_start++;
  char *col_sep = strnchr(db_start, '.', 2);
  if (col_sep == NULL) {
    return NULL;
  }

  size_t name_len = col_sep - db_start;
  char path[MAX_SIZE_NAME * 3];
  name_len = min_unsig(name_len, MAX_SIZE_NAME * 3 - 1);
  strncpy(path, db_start, name_len);
  path[name_len] = '\0';

  char *req = MALLOC(MAX_SIZE_NAME * 4);
  snprintf(req, MAX_SIZE_NAME * 4, "rebuild_indexes(%s)", path);
  return req;
}

// If the input is a load command, pre-parses the given file into
// chunks the server can handle.
bool preprocess_load(char *input, Vec *req_buf) {
  const char *LOAD_PREFIX = "load(\"";
  const char *LOAD_SUFFIX = ")\0";
  const size_t MAX_ROWS_PER_MSG = 50000;

  if (strncmp(input, LOAD_PREFIX, strlen(LOAD_PREFIX)) != 0) {
    return false;
  }

  char *path_start = &input[strlen(LOAD_PREFIX)];
  char *path_end = strchr(path_start, '"');
  if (path_end == NULL) {
    return false;
  }
  *path_end = '\0';

  int fd = open(path_start, O_RDONLY, S_IRUSR);
  if (fd < 0) {
    perror("fopen");
    return false;
  }

  struct stat st;
  if (stat(path_start, &st) != 0) {
    perror("stat");
    return false;
  }

  char *csv = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (csv == MAP_FAILED) {
    perror("mmap");
    log_err("Failed to mmap file\n");
    return false;
  }

  char *end_of_header = strchr(csv, '\n');
  if (end_of_header == NULL) {
    log_err("CSV doesn't have any newlines?");
    return false;
  }

  // Does not include newline
  size_t header_len = end_of_header - csv;
  char *curr = end_of_header + 1;
  while (*curr != '\0') {
    char *last_row_newline = strnchr(curr, '\n', MAX_ROWS_PER_MSG);
    size_t char_ct = last_row_newline == NULL
                         ? strlen(curr)
                         : (size_t)(last_row_newline + 1 - curr);

    char *req = MALLOC(char_ct + header_len + 64);
    size_t req_idx = 0;

    memcpy(&req[req_idx], LOAD_PREFIX, strlen(LOAD_PREFIX));
    req_idx += strlen(LOAD_PREFIX) - 1; // Overwrite the opening quote

    memcpy(&req[req_idx], csv, header_len);
    req_idx += header_len;
    req[req_idx++] = '\n';

    memcpy(&req[req_idx], curr, char_ct);
    req_idx += char_ct;

    memcpy(&req[req_idx], LOAD_SUFFIX, strlen(LOAD_SUFFIX));
    req_idx += strlen(LOAD_SUFFIX);

    req[req_idx] = '\0';
    vec_push(req_buf, (Generic){.ptr = req});

    if (last_row_newline == NULL) {
      break;
    }
    curr = last_row_newline + 1;
  }

  char *rebuild_indexes = make_rebuild_indexes_cmd(req_buf);
  if (rebuild_indexes) {
    vec_push(req_buf, (Generic){.ptr = rebuild_indexes});
  } else {
    log_err("Failed to attach rebuild index cmd");
  }

  if (munmap(csv, st.st_size) < 0) {
    perror("munmap");
    exit(1);
  }

  // The values will be popped. Reverse them to match the order
  // the user expects from the csv.
  vec_reverse(req_buf);
  return true;
}

/// Reserves space in a BatchSelectAcc for at least char_ct new characters.
void bsel_reserve_chars(BatchSelectAcc *acc, size_t char_ct) {
  if (char_ct == 0) {
    return;
  }
  if (acc->cmd == NULL) {
    acc->cmd = MALLOC(64);
    acc->cmd_cap = 64;
    acc->cmd_len = 0;
  }
  if (acc->cmd_len + char_ct < acc->cmd_cap) {
    return;
  }
  // Always leave space for a closing parenthesis, null terminator, and my human
  // fallibility.
  size_t new_cap = max_unsig(acc->cmd_cap * 2, acc->cmd_len + char_ct + 4);
  acc->cmd = REALLOC(acc->cmd, new_cap);
  acc->cmd_cap = new_cap;
}

void bsel_push_chars(BatchSelectAcc *acc, char *str) {
  size_t len = strlen(str);
  bsel_reserve_chars(acc, len);
  memcpy(&acc->cmd[acc->cmd_len], str, len);
  acc->cmd_len += len;
}

void bsel_free(BatchSelectAcc *acc) {
  if (acc->cmd != NULL) {
    free(acc->cmd);
  }
}

BatchSelectAcc bsel_new() {
  return (BatchSelectAcc){
      .cmd = NULL, .cmd_len = 0, .cmd_cap = 0, .has_path = false};
}

bool preprocess_batch_select(char *input, BatchSelectAcc *acc, Vec *req_buf) {
  // PARSE START
  if (acc->cmd_len == 0) {
    if (strncmp(input, "batch_queries()", strlen("batch_queries()")) != 0) {
      return false;
    }
    acc->has_path = false;
    bsel_push_chars(acc, "batch_select(");
    return true;
  }

  // PARSE FINISH
  if (strncmp(input, "batch_execute()", strlen("batch_execute()")) == 0) {
    if (acc->has_path == false) {
      // Nothing to execute!
      acc->cmd_len = 0;
      acc->has_path = false;
      return true;
    }

    // Overwrite the last comma
    acc->cmd[acc->cmd_len - 1] = ')';
    acc->cmd[acc->cmd_len] = '\0';
    vec_push(req_buf, (Generic){.ptr = acc->cmd});

    (*acc) = bsel_new();
    return true;
  }

  // PARSE SELECT
  char *chandle = input;

  char *operator= chandle + next_token(chandle, '=');
  if (operator== chandle) {
    return true;
  }

  char *path = NULL;
  if ((path = first_char_after_match(operator, "select(")) == NULL) {
    log_err("SKIPPING: batch only supports select operations: %s\n", input);
    return true;
  }

  char *min = path + next_token(path, ',');
  if (min == path) {
    return true;
  }

  char *max = min + next_token(min, ',');
  if (min == max) {
    return true;
  }
  trim_parenthesis(max);

  if (!acc->has_path) {
    strncpy(acc->path, path, MAX_SIZE_DB_PATH);
    acc->path[MAX_SIZE_DB_PATH - 1] = '\0';

    bsel_push_chars(acc, acc->path);
    bsel_push_chars(acc, ",");

    acc->has_path = true;
  }

  if (strcmp(acc->path, path) != 0) {
    log_err("SKIPPING: batch sel must select from the same column: %s, %s",
            acc->path, path);
    return true;
  }

  const size_t BUF_SIZE = MAX_SIZE_DB_PATH + MAX_SIZE_INT + MAX_SIZE_INT + 4;
  char buf[BUF_SIZE];
  buf[0] = '\0';
  snprintf(buf, BUF_SIZE, "%s,%s,%s,", chandle, min, max);
  bsel_push_chars(acc, buf);
  return true;
}
