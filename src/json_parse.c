#include "include/json_parse.h"
#include "include/mem.h"
#include "include/utils.h"
#include "include/vector.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>

JsonVal pr_json_parse(char **str);

/// Skips `str` past all characters that trigger `isspace`.
void pr_skip_whitespace(char **str) {
  while (isspace(*str[0]) != 0) {
    (*str)++;
  }
}

/// Extracts a json string type, which is any value wrapped in double
/// quotes. Returns JSON_INVALID on failure.
JsonVal pr_json_parse_str(char **str) {
  if (*str[0] != '"') {
    log_err("json str expected an initial double quote: %s\n", *str);
    return (JsonVal){.type = JSON_INVALID};
  }
  char *end_quote = strchr(++(*str), '"');
  if (end_quote == NULL) {
    log_err("json str double quote wasn't closed: %s\n", *str);
    return (JsonVal){.type = JSON_INVALID};
  }

  size_t string_len = end_quote - *str + 1;
  char *new_str = MALLOC(string_len);
  strncpy(new_str, *str, string_len - 1);
  new_str[string_len - 1] = '\0';

  *str = end_quote + 1;
  return (JsonVal){.type = JSON_STR, .val = (JsonValUnion){.str = new_str}};
}

/// Extracts a json number type. Returns JSON_INVALID on failure.
JsonVal pr_json_parse_num(char **str) {
  char *start = *str;
  double d = strtod(start, str);
  if (*str == start) {
    // Wasn't able to create a double from that at all.
    return (JsonVal){.type = JSON_INVALID};
  }
  return (JsonVal){.type = JSON_NUM, .val = (JsonValUnion){.num = d}};
}

/// Extracts a json object type. Returns JSON_INVALID on failure.
JsonVal pr_json_parse_obj(char **str) {
  if (*str[0] != '{') {
    log_err("json obj parse received a non object string: %s\n", *str);
    return (JsonVal){.type = JSON_INVALID};
  }
  (*str)++;

  JsonVal obj =
      (JsonVal){.type = JSON_OBJ, .val = (JsonValUnion){.obj = vec_new(0)}};
  Vec *kvs = &obj.val.obj;

  while (*str[0] != '\0') {
    pr_skip_whitespace(str);
    if (*str[0] == ',') {
      (*str)++;
      continue;
    }

    if (*str[0] == '}') {
      (*str)++;
      return obj;
    }

    JsonVal key_name = pr_json_parse_str(str);
    if (key_name.type != JSON_STR) {
      json_free(&obj);
      json_free(&key_name);
      return (JsonVal){.type = JSON_INVALID};
    }

    pr_skip_whitespace(str);
    if (*str[0] != ':') {
      log_err("Keyname should be followed by a colon: %s\n", *str);
      json_free(&obj);
      json_free(&key_name);
      return (JsonVal){.type = JSON_INVALID};
    }

    (*str)++; // Skip colon
    JsonObjEntry entry = (JsonObjEntry){.key = key_name.val.str};
    entry.val = pr_json_parse(str);
    if (entry.val.type == JSON_INVALID) {
      json_free(&obj);
      json_free(&key_name);
      return (JsonVal){.type = JSON_INVALID};
    }

    JsonVal *boxed = MALLOC(sizeof(JsonObjEntry));
    memcpy(boxed, &entry, sizeof(JsonObjEntry));
    vec_push(kvs, (Generic){.ptr = boxed});
  }

  log_err("Json object wasn't properly terminated\n");
  json_free(&obj);
  return (JsonVal){.type = JSON_INVALID};
}

/// Extracts a json array type. Returns JSON_INVALID on error.
JsonVal pr_json_parse_arr(char **str) {
  if (*str[0] != '[') {
    log_err("json arr parse received a non array string: %s\n", *str);
    return (JsonVal){.type = JSON_INVALID};
  }
  (*str)++;

  JsonVal lst =
      (JsonVal){.type = JSON_ARR, .val = (JsonValUnion){.arr = vec_new(0)}};
  Vec *entries = &lst.val.arr;

  while (*str[0] != '\0') {
    pr_skip_whitespace(str);
    if (*str[0] == ']') {
      (*str)++;
      return lst;
    }
    JsonVal entry = pr_json_parse(str);
    if (entry.type == JSON_INVALID) {
      json_free(&lst);
      return (JsonVal){.type = JSON_INVALID};
    }
    JsonVal *boxed = MALLOC(sizeof(JsonVal));
    memcpy(boxed, &entry, sizeof(JsonVal));
    vec_push(entries, (Generic){.ptr = boxed});
    if (*str[0] == ',') {
      (*str)++;
    }
  }

  log_err("Json list wasn't terminated\n");
  json_free(&lst);
  return (JsonVal){.type = JSON_INVALID};
}

/// Parses the given pointer into any valid json string. This an all
/// child methods taking `char**` move the string pointer to wherever
/// they finish parsing.
JsonVal pr_json_parse(char **str) {
  pr_skip_whitespace(str);
  if (*str[0] == '"') {
    return pr_json_parse_str(str);
  }
  if (*str[0] == '{') {
    return pr_json_parse_obj(str);
  }
  if (*str[0] == '[') {
    return pr_json_parse_arr(str);
  }
  if (strncmp(*str, "null", 4) == 0) {
    *str += 4;
    return (JsonVal){.type = JSON_NULL};
  }
  // Telling if it's a number is equivalent to just parsing the number.
  JsonVal try_num = pr_json_parse_num(str);
  if (try_num.type == JSON_NUM) {
    return try_num;
  }
  log_err("Json string did not match any types: %s\n", *str);
  return (JsonVal){.type = JSON_INVALID};
}

JsonVal json_parse(char *str) { return pr_json_parse(&str); }

void json_free(JsonVal *json) {
  JsonValEnum jtype = json->type;
  switch (jtype) {
  case JSON_INVALID:
  case JSON_NULL:
  case JSON_NUM: {
    return;
  }
  case JSON_STR: {
    free(json->val.str);
    return;
  }
  case JSON_ARR: {
    Vec *nested = &json->val.arr;
    for (size_t idx = 0; idx < nested->len; idx++) {
      json_free(nested->arr[idx].ptr);
      free(nested->arr[idx].ptr);
    }
    vec_free(nested);
    return;
  }
  case JSON_OBJ: {
    Vec *nested = &json->val.obj;
    for (size_t idx = 0; idx < nested->len; idx++) {
      JsonObjEntry *kv = nested->arr[idx].ptr;
      free(kv->key);
      json_free(&kv->val);
      free(nested->arr[idx].ptr);
    }
    vec_free(nested);
    return;
  }
  }
  log_err("JSON free should have matched a type before reaching the end");
  assert(0);
}

JsonVal *json_obj_get(JsonVal *json_obj, char *key) {
  if (json_obj->type != JSON_OBJ) {
    // log_err("Cannot retrieve kv from non-object type %d\n", json_obj->type);
    return NULL;
  }

  Vec *obj = &json_obj->val.obj;
  for (size_t idx = 0; idx < obj->len; idx++) {
    JsonObjEntry *entry = obj->arr[idx].ptr;
    if (strcmp(entry->key, key) == 0) {
      return &entry->val;
    }
  }

  return NULL;
}
