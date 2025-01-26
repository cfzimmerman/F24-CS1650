#include "include/json_parse.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

void test_str_parse() {
  JsonVal parsed = json_parse("\n\"simple string\"");
  assert(parsed.type == JSON_STR);
  assert(strcmp(parsed.val.str, "simple string") == 0);
  json_free(&parsed);
}

void test_num_parse() {
  JsonVal neg = json_parse("-1.223");
  assert(neg.type == JSON_NUM);
  assert(neg.val.num == -1.223);
  json_free(&neg);

  JsonVal pos = json_parse("54321");
  assert(pos.type == JSON_NUM);
  assert(pos.val.num == 54321);
  json_free(&pos);
}

void test_obj_flat_parse() {
  char *input = "{"
                "\"k1\" : \"val 1\","
                "\"k2\" : \"val 2\","
                "\"k3\": \"val 3\""
                "}";
  JsonVal parsed = json_parse(input);
  assert(parsed.type == JSON_OBJ);

  JsonVal *v1 = json_obj_get(&parsed, "k1");
  assert(v1 != NULL && v1->type == JSON_STR);
  assert(strcmp(v1->val.str, "val 1") == 0);

  JsonVal *v2 = json_obj_get(&parsed, "k2");
  assert(v2 != NULL && v2->type == JSON_STR);
  assert(strcmp(v2->val.str, "val 2") == 0);

  JsonVal *v3 = json_obj_get(&parsed, "k3");
  assert(v3 != NULL && v3->type == JSON_STR);
  assert(strcmp(v3->val.str, "val 3") == 0);

  json_free(&parsed);
}

void test_obj_nested_parse() {
  char *input = "{"
                "\"k1\": \"val 1\","
                "\"obj\": {"
                "\"subk1\": 11,"
                "\"subk2\": -21.2,"
                "\"subk3\": \"val 3\""
                "}"
                "}";

  JsonVal parsed = json_parse(input);
  assert(parsed.type == JSON_OBJ);

  JsonVal *k1 = json_obj_get(&parsed, "k1");
  assert(k1 != NULL && k1->type == JSON_STR);
  assert(strcmp(k1->val.str, "val 1") == 0);

  JsonVal *obj = json_obj_get(&parsed, "obj");
  assert(obj != NULL && obj->type == JSON_OBJ);

  JsonVal *subk1 = json_obj_get(obj, "subk1");
  assert(subk1 != NULL && subk1->type == JSON_NUM);
  assert((int)subk1->val.num == 11);

  JsonVal *subk2 = json_obj_get(obj, "subk2");
  assert(subk2 != NULL && subk2->type == JSON_NUM);
  assert(subk2->val.num == -21.2);

  JsonVal *subk3 = json_obj_get(obj, "subk3");
  assert(subk3 != NULL && subk3->type == JSON_STR);
  assert(strcmp(subk3->val.str, "val 3") == 0);

  json_free(&parsed);
}

void test_arr_flat_parse() {
  JsonVal list = json_parse("[1, -214, \"road\"]");
  assert(list.type == JSON_ARR);
  assert(list.val.arr.len == 3);

  JsonVal *v1 = list.val.arr.arr[0].ptr;
  JsonVal *v2 = list.val.arr.arr[1].ptr;
  JsonVal *v3 = list.val.arr.arr[2].ptr;

  assert(v1->type == JSON_NUM);
  assert((size_t)v1->val.num == 1);

  assert(v2->type == JSON_NUM);
  assert((int)v2->val.num == -214);

  assert(v3->type == JSON_STR);
  assert(strcmp(v3->val.str, "road") == 0);

  json_free(&list);
}

void test_arr_nested_parse() {
  char *input = "[{ \"k1\": [\"val1\"], \"k2\": null }]";
  JsonVal parsed = json_parse(input);
  assert(parsed.type == JSON_ARR);

  // "[{ \"k1\": [\"val1\"], }]"
  Vec *vals = &parsed.val.arr;
  assert(vals->len == 1);

  // "{ "k1": ["val1"], "k2": null }"
  JsonVal *obj = vals->arr[0].ptr;
  assert(obj->type == JSON_OBJ);
  Vec *obj_kv = &obj->val.obj;
  assert(obj_kv->len == 2);

  JsonObjEntry *pair1 = obj_kv->arr[0].ptr;
  assert(strcmp(pair1->key, "k1") == 0);
  assert(pair1->val.type == JSON_ARR);

  // [\"val1\"]
  Vec *sub_arr = &pair1->val.val.arr;
  assert(sub_arr->len == 1);

  // "val1"
  JsonVal *only_val = sub_arr->arr[0].ptr;
  assert(only_val->type == JSON_STR);
  assert(strcmp(only_val->val.str, "val1") == 0);

  JsonObjEntry *pair2 = obj_kv->arr[1].ptr;
  assert(strcmp(pair2->key, "k2") == 0);
  assert(pair2->val.type == JSON_NULL);

  json_free(&parsed);
}

int main() {
  test_str_parse();
  test_num_parse();
  test_obj_flat_parse();
  test_obj_nested_parse();
  test_arr_flat_parse();
  test_arr_nested_parse();

  printf("✅ %s\n", __FILE__);
  return 0;
}
