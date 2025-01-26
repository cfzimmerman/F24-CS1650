#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

/// trims parenthesis characters from a string (in place)
char *trim_parenthesis(char *str);

/// trims whitespace characters from a string (in place)
char *trim_whitespace(char *str);

/// Removes space characters from the given string, shifting
/// other chars forward.
char *trim_spaces(char *str);

/// trims quotations characters from a string (in place)
char *trim_quotes(char *str);

/// Removes `--comment` lines from a string.
void trim_comment(char *str);

/// If the next characters in `query` match `word`, then return
/// a pointer to the first character after that `word` in `query`.
/// Else, returns NULL.
char *first_char_after_match(char *query, char *word);

// cs165_log(out, format, ...)
// Writes the string from @format to the @out pointer, extendable for
// additional parameters.
//
// Usage: cs165_log(stderr, "%s: error at line: %d", __func__, __LINE__);
void cs165_log(FILE *out, const char *format, ...);

// log_err(format, ...)
// Writes the string from @format to stderr, extendable for
// additional parameters. Like cs165_log, but specifically to stderr.
//
// Usage: log_err("%s: error at line: %d", __func__, __LINE__);
void log_err(const char *format, ...);

// log_info(format, ...)
// Writes the string from @format to stdout, extendable for
// additional parameters. Like cs165_log, but specifically to stdout.
// Only use this when appropriate (e.g., denoting a specific checkpoint),
// else defer to using printf.
//
// Usage: log_info("Command received: %s", command_string);
void log_info(const char *format, ...);

// Max and min functions

size_t max_unsig(size_t n1, size_t n2);
int32_t max_sig(int32_t n1, int32_t n2);

size_t min_unsig(size_t n1, size_t n2);
int32_t min_sig(int32_t n1, int32_t n2);

/// Returns the nth instance of the target character in str.
/// Returns str if nth == 0 and NULL if there are fewer than nth
/// instances of that char in the string.
char *strnchr(char *str, char target, size_t nth);

// Str copies at most MAX_SIZE_NAME - 1 characters from src into
// buf, ensuring there's a null terminator at the end.
void copy_name(char *dest, const char *src);

typedef enum StrLabel { STR_STATIC, STR_DYNAMIC } StrLabel;

// A string wrapper that indicates when the char* should be freed.
typedef struct {
  char *str;
  StrLabel type;
} SaferStr;

#endif
