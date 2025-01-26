#include "./include/utils.h"
#include "include/api2.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define LOG 1
#define LOG_ERR 1
#define LOG_INFO 1

void copy_name(char *dest, const char *src) {
  strncpy(dest, src, MAX_SIZE_NAME - 1);
  dest[MAX_SIZE_NAME - 1] = '\0';
}

size_t max_unsig(size_t n1, size_t n2) {
  if (n1 >= n2) {
    return n1;
  }
  return n2;
}

int32_t max_sig(int32_t n1, int32_t n2) {
  if (n1 >= n2) {
    return n1;
  }
  return n2;
}

size_t min_unsig(size_t n1, size_t n2) {
  if (n1 <= n2) {
    return n1;
  }
  return n2;
}

int32_t min_sig(int32_t n1, int32_t n2) {
  if (n1 <= n2) {
    return n1;
  }
  return n2;
}

/* removes space characters from the input string.
 * Shifts characters over and shortens the length of
 * the string by the number of space characters.
 */
char *trim_whitespace(char *str) {
  int length = strlen(str);
  int current = 0;
  for (int i = 0; i < length; ++i) {
    if (!isspace(str[i])) {
      str[current++] = str[i];
    }
  }

  // Write new null terminator
  str[current] = '\0';
  return str;
}

char *trim_spaces(char *str) {
  int length = strlen(str);
  int current = 0;
  for (int i = 0; i < length; ++i) {
    if (!(str[i] == ' ')) {
      str[current++] = str[i];
    }
  }

  // Write new null terminator
  str[current] = '\0';
  return str;
}

/* removes parenthesis characters from the input string.
 * Shifts characters over and shortens the length of
 * the string by the number of parenthesis characters.
 */
char *trim_parenthesis(char *str) {
  int length = strlen(str);
  int current = 0;
  for (int i = 0; i < length; ++i) {
    if (!(str[i] == '(' || str[i] == ')')) {
      str[current++] = str[i];
    }
  }

  // Write new null terminator
  str[current] = '\0';
  return str;
}

char *trim_quotes(char *str) {
  int length = strlen(str);
  int current = 0;
  for (int i = 0; i < length; ++i) {
    if (str[i] != '\"') {
      str[current++] = str[i];
    }
  }

  // Write new null terminator
  str[current] = '\0';
  return str;
}

void trim_comment(char *str) {
  char *comment = strstr(str, "--");
  if (comment != NULL) {
    *comment = '\0';
  }
}

char *strnchr(char *str, char target, size_t nth) {
  if (nth == 0) {
    return str;
  }
  char *prev = strchr(str, target);
  for (size_t ct = 1; ct < nth; ct++) {
    if (prev == NULL) {
      return NULL;
    }
    prev++;
    prev = strchr(prev, target);
  }
  return prev;
}

/// If the next characters in `query` match `word`, then return
/// a pointer to the first character after that `word` in `query`.
/// Else, returns NULL.
char *first_char_after_match(char *query, char *word) {
  size_t word_len = strlen(word);
  if (strncmp(query, word, word_len) == 0) {
    return query + word_len;
  }
  return NULL;
}

/* The following three functions will show output on the terminal
 * based off whether the corresponding level is defined.
 * To see log output, define LOG.
 * To see error output, define LOG_ERR.
 * To see info output, define LOG_INFO
 */
void cs165_log(FILE *out, const char *format, ...) {
#ifdef LOG
  va_list v;
  va_start(v, format);
  vfprintf(out, format, v);
  va_end(v);
#else
  (void)out;
  (void)format;
#endif
}

void log_err(const char *format, ...) {
#ifdef LOG_ERR
  va_list v;
  va_start(v, format);
  fprintf(stderr, ANSI_COLOR_RED);
  vfprintf(stderr, format, v);
  fprintf(stderr, ANSI_COLOR_RESET);
  va_end(v);
#else
  (void)format;
#endif
}

void log_info(const char *format, ...) {
#ifdef LOG_INFO
  va_list v;
  va_start(v, format);
  fprintf(stdout, ANSI_COLOR_GREEN);
  vfprintf(stdout, format, v);
  fprintf(stdout, ANSI_COLOR_RESET);
  fflush(stdout);
  va_end(v);
#else
  (void)format;
#endif
}
