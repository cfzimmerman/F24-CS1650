#include "include/client_context.h"
#include "include/bitvec.h"
#include "include/mem.h"
#include "include/utils.h"
#include "include/vector.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

Chandle *chandle_set(ClientCxt2 *cxt, char *chname, Chandle partial_chandle) {
  chandle_del(cxt, chname);
  Chandle *ch = MALLOC(sizeof(Chandle));
  *ch = partial_chandle;
  copy_name(ch->chname, chname);

  vec_push(&cxt->handles, (Generic){.ptr = ch});
  return ch;
}

Chandle *chandle_set_bitvec(ClientCxt2 *cxt, char *chname, Column2 *column,
                            BitVec bitvec) {
  chandle_del(cxt, chname);

  Chandle *ch = MALLOC(sizeof(Chandle));
  *ch = (Chandle){.type = CHANDLE_BITVEC,
                  .val = {.bv = (ChandleBitVec){
                              .bitvec = bitvec,
                              .column = column,
                          }}};
  copy_name(ch->chname, chname);

  vec_push(&cxt->handles, (Generic){.ptr = ch});
  return ch;
}

Chandle *chandle_set_intvec(ClientCxt2 *cxt, char *chname, int *nums,
                            size_t len) {
  chandle_del(cxt, chname);

  Chandle *ch = MALLOC(sizeof(Chandle));
  *ch = (Chandle){.type = CHANDLE_INTVEC,
                  .val = {.iv = (ChandleIntVec){.nums = nums, .len = len}}};
  strncpy(ch->chname, chname, MAX_SIZE_NAME - 1);
  ch->chname[MAX_SIZE_NAME - 1] = '\0';

  vec_push(&cxt->handles, (Generic){.ptr = ch});
  return ch;
}

Chandle *chandle_set_aggregate(ClientCxt2 *cxt, char *chname, double val,
                               AggPrecision precision) {
  chandle_del(cxt, chname);

  Chandle *ch = MALLOC(sizeof(Chandle));
  *ch = (Chandle){
      .type = CHANDLE_AGGREGATE,
      .val = {.agg = (ChandleAggregate){.val = val, .prec = precision}}};
  strncpy(ch->chname, chname, MAX_SIZE_NAME - 1);
  ch->chname[MAX_SIZE_NAME - 1] = '\0';

  vec_push(&cxt->handles, (Generic){.ptr = ch});
  return ch;
}

Chandle *chandle_get(ClientCxt2 *cxt, char *chname) {
  for (size_t idx = 0; idx < cxt->handles.len; idx++) {
    Chandle *ch = cxt->handles.arr[idx].ptr;
    if (strcmp(ch->chname, chname) == 0) {
      return ch;
    }
  }
  return NULL;
}

void chandle_free(Chandle *chandle) {
  switch (chandle->type) {
  case CHANDLE_BITVEC: {
    bitvec_free(&chandle->val.bv.bitvec);
    return;
  }
  case CHANDLE_POSLIST: {
    free(chandle->val.pos.pos);
    return;
  }
  case CHANDLE_INTVEC: {
    free(chandle->val.iv.nums);
    return;
  }
  case CHANDLE_SLICE:
  case CHANDLE_AGGREGATE: {
    return;
  }
  default: {
    log_err("chandle_free didn't match all cases");
    exit(1);
  }
  }
}

void chandle_del(ClientCxt2 *cxt, char *chname) {
  Chandle *chandle = NULL;
  size_t ch_idx = 0;

  for (size_t idx = 0; idx < cxt->handles.len; idx++) {
    Chandle *ch = cxt->handles.arr[idx].ptr;
    if (strcmp(ch->chname, chname) == 0) {
      chandle = ch;
      ch_idx = idx;
      break;
    }
  }
  if (chandle == NULL) {
    return;
  }

  vec_swap(&cxt->handles, ch_idx, cxt->handles.len - 1);
  Chandle *popped = vec_pop(&cxt->handles).ptr;
  assert(popped == chandle);

  chandle_free(chandle);
  free(chandle);
}

void cxt_free(ClientCxt2 *cxt) {
  for (size_t idx = 0; idx < cxt->handles.len; idx++) {
    Chandle *ch = cxt->handles.arr[idx].ptr;
    chandle_free(ch);
    free(ch);
  }
  vec_free(&cxt->handles);
}
