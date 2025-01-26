#ifndef CZ_JSON_PARSE
#define CZ_JSON_PARSE

#include "vector.h"

typedef enum JsonValEnum {
  JSON_STR,
  JSON_NUM,
  JSON_ARR,
  JSON_OBJ,
  JSON_NULL,
  JSON_INVALID
} JsonValEnum;

typedef union JsonValUnion {
  char *str;
  double num;
  Vec arr; // Vec<JsonVal>
  Vec obj; // Vec<JsonObjEntry>
} JsonValUnion;

typedef struct JsonVal {
  JsonValEnum type;
  JsonValUnion val;
} JsonVal;

typedef struct JsonObjEntry {
  char *key;
  JsonVal val;
} JsonObjEntry;

/// Parses a string into json recursively or returns JSON_INVALID on failure.
JsonVal json_parse(char *str);

/// Releases all memory held by this and all child JsonVal instances.
void json_free(JsonVal *val);

/// Indexes into a json object for the requested key.
/// Returns JSON_INVALID on failure.
JsonVal *json_obj_get(JsonVal *json_obj, char *key);

#endif
