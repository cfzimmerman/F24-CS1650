#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "include/api2.h"
#include "include/catalogue.h"
#include "include/client_context.h"
#include "include/operators.h"
#include "include/parse2.h"
#include "include/threadpool.h"
#include "include/utils.h"
#include "include/vector.h"

#define DEFAULT_QUERY_BUFFER_SIZE 1024

/// Executes the given DB command against the current catalogue, returning
/// a user-friendly response.
SaferStr execute_cmd(Catalogue *ctlg, ThreadPool *threads, ParsedCmd *cmd,
                     ClientCxt2 *cxt, char *handle, size_t *thread_ct) {
  switch (cmd->variant) {
  case PARSED_IGNORE: {
    return (SaferStr){.str = "", .type = STR_STATIC};
  }
  case PARSED_CREATE_DB: {
    CreateDb *req = &cmd->cmd.create_db;
    if (ctlg_db_new(ctlg, req->db_name) == NULL) {
      log_err("Failed: ctlg_db_new(ctlg, %s)", req->db_name);
      return (SaferStr){.type = STR_STATIC, .str = "FAILED: CREATE DB"};
    }
    return (SaferStr){.str = "", .type = STR_STATIC};
  }
  case PARSED_CREATE_TABLE: {
    CreateTable *req = &cmd->cmd.create_table;
    if (ctlg_table_new(ctlg, req->db_name, req->table_name, req->column_ct) ==
        NULL) {
      log_err("Failed: ctlg_table_new(ctlg, %s, %s, %d)", req->db_name,
              req->table_name, req->column_ct);
      return (SaferStr){.type = STR_STATIC, .str = "FAILED: CREATE TABLE"};
    }
    return (SaferStr){.str = "", .type = STR_STATIC};
  }
  case PARSED_CREATE_COL: {
    CreateCol *req = &cmd->cmd.create_col;
    if (ctlg_column_new(ctlg, req->db_name, req->table_name, req->col_name,
                        0) == NULL) {
      log_err("Failed: ctlg_table_new(ctlg, %s, %s, %s)", req->db_name,
              req->table_name, req->col_name);
      return (SaferStr){.type = STR_STATIC, .str = "FAILED: CREATE COLUMN"};
    }
    return (SaferStr){.str = "", .type = STR_STATIC};
  }
  case PARSED_INSERT_INTO: {
    InsertInto *req = &cmd->cmd.insert_into;
    Status res =
        db_insert_row(ctlg, req->db_name, req->table_name, &req->values);
    if (res.code == ERROR) {
      return (SaferStr){.type = STR_STATIC, .str = res.error_message};
    }
    return (SaferStr){.str = "", .type = STR_STATIC};
  }
  case PARSED_SELECT_COL: {
    if (handle == NULL) {
      // No point in selecting if we're not saving it anywhere.
      return (SaferStr){.str = "", .type = STR_STATIC};
    }
    SelectCol *req = &cmd->cmd.select_col;
    SelectResult sel =
        db_select_col(ctlg, req->db_name, req->table_name, req->col_name,
                      req->min_incl, req->max_incl, threads, *thread_ct);
    if (sel.status.code == ERROR) {
      return (SaferStr){.str = sel.status.error_message, .type = STR_STATIC};
    }
    chandle_set(cxt, handle, sel.chandle);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_SELECT_CHNAME: {
    if (handle == NULL) {
      return (SaferStr){.str = "", .type = STR_STATIC};
    }
    SelectChname *req = &cmd->cmd.select_chname;
    SelectResult sel =
        db_select_chname(cxt, req->bitvec_filter_chname,
                         req->intvec_nums_chname, req->min_incl, req->max_incl);
    if (sel.status.code == ERROR) {
      return (SaferStr){.str = sel.status.error_message, .type = STR_STATIC};
    }
    chandle_set(cxt, handle, sel.chandle);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_FETCH_INT: {
    if (handle == NULL) {
      return (SaferStr){.str = "", .type = STR_STATIC};
    }
    FetchInt *req = &cmd->cmd.fetch_int;
    FetchResult fet = db_fetch(ctlg, cxt, req->db_name, req->table_name,
                               req->col_name, req->chname);
    if (fet.status.code == ERROR) {
      return (SaferStr){.str = fet.status.error_message, .type = STR_STATIC};
    }
    chandle_set_intvec(cxt, handle, fet.results.nums, fet.results.len);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_PRINT: {
    PrintVals *req = &cmd->cmd.print_vals;
    PrintResult res = db_print(ctlg, cxt, &req->printable);
    if (res.status.code == ERROR) {
      return (SaferStr){.str = res.status.error_message, .type = STR_STATIC};
    }
    return res.str;
  }
  case PARSED_LOAD: {
    LoadCsv *req = &cmd->cmd.load_csv;
    Status res = db_load(ctlg, req->db_name, req->table_name, &req->cols);
    if (res.code == ERROR) {
      return (SaferStr){.str = res.error_message, .type = STR_STATIC};
    }
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_REBUILD_INDEXES: {
    RebuildIndexes *req = &cmd->cmd.rebuild_indexes;
    return db_rebuild_indexes(ctlg, req->db_name, req->table_name);
  }
  case PARSED_SHUTDOWN: {
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_SUM: {
    if (handle == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = ""};
    }
    AggRes res = db_sum(ctlg, cxt, &cmd->cmd.sum);
    if (res.status.code == ERROR) {
      return (SaferStr){.type = STR_STATIC, .str = res.status.error_message};
    }
    chandle_set_aggregate(cxt, handle, res.val, AGG_INTEGER);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_AVG: {
    if (handle == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = ""};
    }
    AggRes res = db_avg(ctlg, cxt, &cmd->cmd.avg);
    if (res.status.code == ERROR) {
      return (SaferStr){.type = STR_STATIC, .str = res.status.error_message};
    }
    chandle_set_aggregate(cxt, handle, res.val, AGG_REAL);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_ADD: {
    if (handle == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = ""};
    }
    ParseDualAggregate *agg = &cmd->cmd.add;
    FetchResult added = db_add(ctlg, cxt, &agg->left, &agg->right);
    if (added.status.code == ERROR) {
      return (SaferStr){.str = added.status.error_message, .type = STR_STATIC};
    }
    chandle_set_intvec(cxt, handle, added.results.nums, added.results.len);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_SUB: {
    // Basically everything about sub is copy-paste from add. They can probably
    // be merged together when I have time.
    if (handle == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = ""};
    }
    ParseDualAggregate *agg = &cmd->cmd.sub;
    FetchResult sub = db_sub(ctlg, cxt, &agg->left, &agg->right);
    if (sub.status.code == ERROR) {
      return (SaferStr){.str = sub.status.error_message, .type = STR_STATIC};
    }
    chandle_set_intvec(cxt, handle, sub.results.nums, sub.results.len);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_MIN_SINGLE: {
    if (handle == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = ""};
    }
    ParseAggregate *agg = &cmd->cmd.min_single;
    AggRes min = db_min_single(ctlg, cxt, agg);
    if (min.status.code == ERROR) {
      return (SaferStr){.str = min.status.error_message, .type = STR_STATIC};
    }
    chandle_set_aggregate(cxt, handle, min.val, AGG_INTEGER);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_MAX_SINGLE: {
    if (handle == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = ""};
    }
    ParseAggregate *agg = &cmd->cmd.max_single;
    AggRes max = db_max_single(ctlg, cxt, agg);
    if (max.status.code == ERROR) {
      return (SaferStr){.str = max.status.error_message, .type = STR_STATIC};
    }
    chandle_set_aggregate(cxt, handle, max.val, AGG_INTEGER);
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_BATCH_SELECT_COL: {
    BatchSelectCol *queries = &cmd->cmd.batch_select;
    BatchSelectQueryResult res = db_select_batch(
        ctlg, queries->db_name, queries->table_name, queries->col_name,
        &queries->filters, threads, *thread_ct);

    if (res.status.code == ERROR) {
      return (SaferStr){.str = res.status.error_message, .type = STR_STATIC};
    }
    for (size_t idx = 0; idx < queries->filters.len; idx++) {
      SelectColFilter *filter = queries->filters.arr[idx].ptr;
      chandle_set_bitvec(cxt, filter->handle, res.col, res.queries[idx].result);
    }
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_SINGLE_THREAD_START: {
    *thread_ct = 1;
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_SINGLE_THREAD_STOP: {
    *thread_ct = threads->thread_ct;
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_CREATE_IDX: {
    CreateIndex *create = &cmd->cmd.create_idx;
    if (ctlg_index_new(ctlg, create->db_name, create->table_name,
                       create->col_name, create->type) == NULL) {
      return (SaferStr){.type = STR_STATIC, .str = "Create index failed"};
    }
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  case PARSED_JOIN: {
    ParseJoin *join = &cmd->cmd.join;
    JoinResult res = db_join(cxt, join, threads);

    if (res.status.code == ERROR) {
      return (SaferStr){.type = STR_STATIC, .str = res.status.error_message};
    }
    chandle_set(cxt, join->out_chname1,
                (Chandle){.type = CHANDLE_POSLIST, .val = {.pos = res.pos1}});
    chandle_set(cxt, join->out_chname2,
                (Chandle){.type = CHANDLE_POSLIST, .val = {.pos = res.pos2}});
    return (SaferStr){.type = STR_STATIC, .str = ""};
  }
  }

  assert(false);
}

typedef enum ClientExitStatus { CLEX_ABRUPT, CLEX_SHUTDOWN } ClientExitStatus;

/**
 * handle_client(client_socket)
 * This is the execution routine after a client has connected.
 * It will continually listen for messages from the client and execute queries.
 **/
ClientExitStatus handle_client(int client_socket, Catalogue *ctlg,
                               ThreadPool *threads) {
  log_info("Connected to socket: %d.\n", client_socket);

  IoMessage send_message;
  IoMessage recv_message;

  size_t thread_ct = threads->thread_ct;
  ClientCxt2 cxt = (ClientCxt2){.handles = vec_new(8)};

  // Continually receive messages from client and execute queries.
  // 1. Parse the command
  // 2. Handle request if appropriate
  // 3. Send status of the received message (OK, UNKNOWN_QUERY, etc)
  // 4. Send response to the request.
  ClientExitStatus exit_status = CLEX_ABRUPT;
  bool done = false;
  while (!done) {
    ssize_t frame_len =
        recv(client_socket, &recv_message, sizeof(IoMessage), 0);
    if (frame_len < 0) {
      log_err("recv returned frame_len <= 0: %d\n", frame_len);
      break;
    }
    if (frame_len == 0) {
      // Client disconnected
      break;
    }

    char recv_buffer[recv_message.length + 1];
    ssize_t message_len =
        recv(client_socket, recv_buffer, recv_message.length, MSG_WAITALL);
    if (message_len < 0 || message_len != recv_message.length) {
      log_err("recv returned message_len <= 0: %d\n", message_len);
      break;
    }

    recv_message.payload = recv_buffer;
    recv_message.payload[recv_message.length] = '\0';

    ParsedCmd query = parse_cmd(recv_message.payload);
    SaferStr result =
        execute_cmd(ctlg, threads, &query, &cxt, query.handle, &thread_ct);
    parsed_cmd_free(&query);

    send_message.length = strlen(result.str);
    char send_buffer[send_message.length + 1];
    strcpy(send_buffer, result.str);
    send_message.payload = send_buffer;
    send_message.status = OK_WAIT_FOR_RESPONSE;

    if (send(client_socket, &(send_message), sizeof(IoMessage), MSG_NOSIGNAL) ==
        -1) {
      log_err("Failed to send message.\n");
      done = true;
    }

    if (!done && send(client_socket, result.str, send_message.length,
                      MSG_NOSIGNAL) == -1) {
      log_err("Failed to send message.\n");
      done = true;
    }

    if (result.type == STR_DYNAMIC) {
      free(result.str);
    }

    if (query.variant == PARSED_SHUTDOWN) {
      log_info("Received shutdown command.\n");
      done = true;
      exit_status = CLEX_SHUTDOWN;
    }
  }

  cxt_free(&cxt);
  close(client_socket);
  log_info("Connection closed at socket %d!\n", client_socket);
  log_info("Exit status: %d\n", exit_status);

  return exit_status;
}

/**
 * setup_server()
 *
 * This sets up the connection on the server side using unix sockets.
 * Returns a valid server socket fd on success, else -1 on failure.
 **/
int setup_server() {
  int server_socket;
  size_t len;
  struct sockaddr_un local;

  log_info("Attempting to setup server...\n");

  if ((server_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    log_err("L%d: Failed to create socket.\n", __LINE__);
    return -1;
  }

  local.sun_family = AF_UNIX;
  strncpy(local.sun_path, SOCK_PATH, strlen(SOCK_PATH) + 1);
  unlink(local.sun_path);

  len = strlen(local.sun_path) + sizeof(local.sun_family) + 1;
  if (bind(server_socket, (struct sockaddr *)&local, len) == -1) {
    log_err("L%d: Socket failed to bind.\n", __LINE__);
    return -1;
  }

  if (listen(server_socket, 5) == -1) {
    log_err("L%d: Failed to listen on socket.\n", __LINE__);
    return -1;
  }

  return server_socket;
}

// Currently this main will setup the socket and accept a single client.
// After handling the client, it will exit.
// You WILL need to extend this to handle MULTIPLE concurrent clients
// and remain running until it receives a shut-down command.
//
// Getting Started Hints:
//      How will you extend main to handle multiple concurrent clients?
//      Is there a maximum number of concurrent client connections you will
//      allow? What aspects of siloes or isolation are maintained in your
//      design? (Think `what` is shared between `whom`?)
int main(void) {
  int server_socket = setup_server();
  if (server_socket < 0) {
    exit(1);
  }

  char *ctlg_file = DB_PATH_PREFIX "catalogue.json";
  char *ctlg_str = ctlg_readf(ctlg_file);

  Catalogue ctlg = (Catalogue){.db = NULL};
  if (ctlg_str != NULL) {
    log_info("Loading catalog file\n");
    ctlg = ctlg_rehydrate(ctlg_str);
  } else {
    log_info("Starting without a pre-existing catalog\n");
  }

  ThreadPool *threads = tpool_new(tpool_suggest_size());

  log_info("Waiting for a connection %d ...\n", server_socket);

  struct sockaddr_un remote;
  socklen_t t = sizeof(remote);
  int client_socket = 0;

  while (true) {
    if ((client_socket =
             accept(server_socket, (struct sockaddr *)&remote, &t)) == -1) {
      log_err("L%d: Failed to accept a new connection.\n", __LINE__);
      exit(1);
    }

    ClientExitStatus status = handle_client(client_socket, &ctlg, threads);
    if (status == CLEX_SHUTDOWN) {
      break;
    }
  }

  log_info("Persisting db\n");
  char *serialized = ctlg_dehydrate(&ctlg);
  if (!ctlg_writef(ctlg_file, serialized)) {
    log_err("Failed to write serialized ctlg: %s\n", serialized);
    exit(1);
  }
  log_info("persisting catalogue: %s\n", serialized);
  tpool_free(threads);
  free(serialized);

  return 0;
}
