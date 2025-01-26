#include "include/client_tools.h"
#include "include/mem.h"
#include "include/vector.h"
#include <assert.h>
#include <string.h>

int main() {
  BatchSelectAcc acc = bsel_new();
  Vec req_buf = vec_new(4);

  char input[512];
  input[511] = '\0';

  {
    // Normal queries are ignored and untouched.
    const char *QUERY =
        "ids_of_top_students=fetch(awesomebase.grades.student_id,a_plus)";
    strcpy(input, QUERY);
    assert(preprocess_batch_select(input, &acc, &req_buf) == false);
    assert(strcmp(input, QUERY) == 0);
  }

  {
    // Parse a few batched selections
    char *queries[5] = {
        "batch_queries()", "s1=select(db1.tbl3_batch.col1,10,20)",
        "s2=select(db1.tbl3_batch.col1,800,830)",
        "s3=select(db1.tbl3_batch.col1,50,500)", "batch_execute()"};
    for (size_t idx = 0; idx < 5; idx++) {
      strcpy(input, queries[idx]);
      assert(preprocess_batch_select(input, &acc, &req_buf));
    }

    assert(acc.has_path == false);
    assert(acc.cmd == NULL);

    assert(req_buf.len == 1);
    const char *EXPECT =
        "batch_select(db1.tbl3_batch.col1,s1,10,20,s2,800,830,s3,50,500)";

    assert(strcmp(req_buf.arr[0].ptr, EXPECT) == 0);
    free(vec_pop(&req_buf).ptr);
    assert(req_buf.len == 0);
  }

  {
    const char *EXPECT =
        "batch_select(db1.tbl3_batch.col1,s1,10,20,s2,800,830,s3,50,500)";
    vec_push(&req_buf, (Generic){.ptr = (void *)EXPECT});

    char *req = make_rebuild_indexes_cmd(&req_buf);
    assert(req);
    assert(strcmp(req, "rebuild_indexes(db1.tbl3_batch)") == 0);
    vec_pop(&req_buf);

    free(req);
  }

  vec_free(&req_buf);
  bsel_free(&acc);

  printf("✅ %s\n", __FILE__);
  return 0;
}
