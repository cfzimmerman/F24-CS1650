/* This line at the top is necessary for compilation on the lab machine and many
other Unix machines. Please look up _XOPEN_SOURCE for more details. As well, if
your code does not compile on the lab machine please look into this as a a
source of error. */
#define _XOPEN_SOURCE

#include "include/vector.h"
#include <assert.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include "include/client_tools.h"
#include "include/mem.h"
#include "include/utils.h"

#define DEFAULT_STDIN_BUFFER_SIZE 1024

/**
 * connect_client()
 *
 * This sets up the connection on the client side using unix sockets.
 * Returns a valid client socket fd on success, else -1 on failure.
 *
 **/
int connect_client() {
  int client_socket;
  size_t len;
  struct sockaddr_un remote;

  log_info("-- Attempting to connect...\n");

  if ((client_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    log_err("L%d: Failed to create socket.\n", __LINE__);
    return -1;
  }

  remote.sun_family = AF_UNIX;
  strncpy(remote.sun_path, SOCK_PATH, strlen(SOCK_PATH) + 1);
  len = strlen(remote.sun_path) + sizeof(remote.sun_family) + 1;
  if (connect(client_socket, (struct sockaddr *)&remote, len) == -1) {
    perror("client connect");
    log_err("client connect failed: ");
    return -1;
  }

  log_info("-- Client connected at socket: %d.\n", client_socket);
  return client_socket;
}

int main(void) {
  int client_socket = connect_client();
  if (client_socket < 0) {
    exit(1);
  }

  // Always output an interactive marker at the start of each command if the
  // input is from stdin. Do not output if piped in from file or from other fd
  char *prefix = "";
  if (isatty(fileno(stdin))) {
    prefix = "db_client > ";
  }

  // Make this a queue if correctness demands
  Vec send_stack = vec_new(8); // Vec<&str>

  // Accumulates batched select operations
  BatchSelectAcc bsel_acc = bsel_new();

  while (true) {
    // SEND

    char *buffered = NULL;
    if (send_stack.len != 0) {
      buffered = vec_pop(&send_stack).ptr;
    }

    char *input = NULL;
    char input_buf[DEFAULT_STDIN_BUFFER_SIZE];
    if (buffered == NULL) {
      printf("%s", prefix);
      input = fgets(input_buf, DEFAULT_STDIN_BUFFER_SIZE, stdin);
      if (feof(stdin)) {
        break;
      }
    }

    if (input != NULL) {
      trim_spaces(input);
      if (preprocess_load(input_buf, &send_stack)) {
        continue;
      }
      if (preprocess_batch_select(input_buf, &bsel_acc, &send_stack)) {
        continue;
      }
    }

    // Send the message_header, which tells the server payload size
    char *payload = buffered ? buffered : input;
    assert(payload != NULL);
    IoMessage tx_msg = (IoMessage){.payload = payload,
                                   .length = strlen(payload),
                                   .status = OK_WAIT_FOR_RESPONSE};
    if (send(client_socket, &tx_msg, sizeof(IoMessage), 0) < 0) {
      log_err("Failed to send message header.");
      exit(1);
    }

    // Send the payload to server
    if (send(client_socket, tx_msg.payload, tx_msg.length, 0) < 0) {
      log_err("Failed to send query payload.");
      exit(1);
    }

    if (buffered != NULL) {
      free(buffered);
    }

    // RECEIVE

    IoMessage rx_msg;
    ssize_t rx_header_res = recv(client_socket, &rx_msg, sizeof(IoMessage), 0);
    if (rx_header_res < 0) {
      log_err("Failed to receive response");
      break;
    }
    if (rx_header_res == 0) {
      log_info("Server closed connection");
      break;
    }

    if (rx_msg.status != OK_WAIT_FOR_RESPONSE && rx_msg.status != OK_DONE) {
      continue;
    }
    if (rx_msg.length == 0) {
      continue;
    }
    char rx_buf[rx_msg.length + 1];
    if (recv(client_socket, &rx_buf, rx_msg.length, MSG_WAITALL) <= 0) {
      log_err("Failed to read %lu declared bytes\n", rx_msg.length);
      break;
    }
    rx_buf[rx_msg.length] = '\0';
    printf("%s\n", rx_buf);
  }

  vec_free(&send_stack);
  bsel_free(&bsel_acc);
  close(client_socket);
  return 0;
}
