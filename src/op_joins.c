#include "include/channel.h"
#include "include/client_context.h"
#include "include/hash_table.h"
#include "include/operators.h"
#include "include/threadpool.h"
#include "include/utils.h"
#include "include/vector.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// An extension of `operators.c` implementing various join algorithms.

ChandlePosList pr_poslist_from_bitvec(BitVec *bv) {
  ChandlePosList pos = {.pos = MALLOC(sizeof(uint64_t) * bv->bit_ones),
                        .len = 0};
  for (size_t major_bit = 0; major_bit < bv->bit_len && pos.len != bv->bit_ones;
       major_bit += 8) {
    size_t byte_idx = major_bit / 8;
    uint8_t curr_byte = bv->bits[byte_idx];
    size_t minor_bit = 0;
    while (curr_byte != 0) {
      pos.pos[pos.len] = major_bit + minor_bit;
      pos.len += (curr_byte & 1);
      curr_byte >>= 1;
      minor_bit++;
    }
  }
  assert(pos.len == bv->bit_ones);
  return pos;
}

ChandlePosList pr_poslist_from_slice(ChandleSlice *slice) {
  ChandlePosList pos = {.pos = MALLOC(sizeof(uint64_t) * slice->len),
                        .len = slice->len};
  for (size_t idx = 0; idx < slice->len; idx++) {
    pos.pos[idx] = slice->start_idx + idx;
  }
  return pos;
}

Status chandle_try_poslist(Chandle *ch) {
  // chandle_free doesn't affect ch->name, so assume its safe to reuse.
  switch (ch->type) {
  case CHANDLE_BITVEC: {
    ChandlePosList pos = pr_poslist_from_bitvec(&ch->val.bv.bitvec);
    chandle_free(ch);
    ch->type = CHANDLE_POSLIST;
    ch->val.pos = pos;
    return status_ok();
  }
  case CHANDLE_SLICE: {
    ChandlePosList pos = pr_poslist_from_slice(&ch->val.slice);
    chandle_free(ch);
    ch->type = CHANDLE_POSLIST;
    ch->val.pos = pos;
    return status_ok();
  }
  case CHANDLE_POSLIST: {
    return status_ok();
  }
  case CHANDLE_INTVEC: {
    return status_err("CHANDLE ERR: Cannot make poslist from intvec\n");
  }
  case CHANDLE_AGGREGATE: {
    return status_err("CHANDLE ERR: Cannot make poslist from aggregate\n");
  }
  default: {
    log_err("Unmatched case in chandle_make_poslist\n");
    exit(1);
  }
  }
}

typedef struct {
  const ChandleIntVec *val;
  const ChandlePosList *pos;
  ChandlePosList *res;
} JoinInput;

void pr_join_copy_results(ChandlePosList *full_res, ChandlePosList *temp_res) {
  full_res->pos = REALLOC(full_res->pos,
                          sizeof(uint64_t) * (full_res->len + temp_res->len));
  memcpy(&full_res->pos[full_res->len], temp_res->pos,
         sizeof(uint64_t) * temp_res->len);
  full_res->len += temp_res->len;
}

// A grace join result buffer. Threads must acquire the lock before
// writing their results and release it when finished.
// Basically a convenience wrapper over pthread calls.
typedef struct {
  pthread_mutex_t lock;
  ChandlePosList *res_outer;
  ChandlePosList *res_inner;
} LockedJoinRes;

LockedJoinRes ljr_new(ChandlePosList *res_outer, ChandlePosList *res_inner) {
  LockedJoinRes ljr = {.res_outer = res_outer, .res_inner = res_inner};
  if (pthread_mutex_init(&ljr.lock, NULL) != 0) {
    perror("pthread_mutex_init");
    exit(1);
  }
  return ljr;
}

void ljr_lock(LockedJoinRes *ljr) {
  if (pthread_mutex_lock(&ljr->lock) != 0) {
    perror("pthread_mutex_lock");
    exit(1);
  }
}

void ljr_unlock(LockedJoinRes *ljr) {
  if (pthread_mutex_unlock(&ljr->lock) != 0) {
    perror("pthread_mutex_unlock");
    exit(1);
  }
}

void ljr_free(LockedJoinRes *ljr) {
  if (pthread_mutex_destroy(&ljr->lock) != 0) {
    perror("pthread_mutex_destroy");
    exit(1);
  }
}

typedef struct {
  LockedJoinRes *ljr;
  const JoinInput *outer;
  const JoinInput *inner;
  size_t outer_start;
  size_t outer_len;
  Channel *finished;
} JoinInnerArgs;

/// Parallelizable task in a nested loop join.
void pr_join_inner_loop(void *join_inner_args) {
  JoinInnerArgs *args = join_inner_args;
  assert(args->outer_start + args->outer_len <= args->outer->pos->len);

  size_t max_len = args->outer_len * args->inner->pos->len;
  ChandlePosList outer_res = {.pos = MALLOC(sizeof(uint64_t) * max_len),
                              .len = 0};
  ChandlePosList inner_res = {.pos = MALLOC(sizeof(uint64_t) * max_len),
                              .len = 0};

  for (size_t out_idx = args->outer_start;
       out_idx < args->outer_start + args->outer_len; out_idx++) {
    for (size_t in_idx = 0; in_idx < args->inner->val->len; in_idx++) {
      if (args->outer->val->nums[out_idx] == args->inner->val->nums[in_idx]) {
        outer_res.pos[outer_res.len++] = args->outer->pos->pos[out_idx];
        inner_res.pos[inner_res.len++] = args->inner->pos->pos[in_idx];
      }
    }
  }

  ljr_lock(args->ljr);
  pr_join_copy_results(args->ljr->res_outer, &outer_res);
  pr_join_copy_results(args->ljr->res_inner, &inner_res);
  ljr_unlock(args->ljr);
  chan_send(args->finished, (Generic){.uint = 0});

  free(outer_res.pos);
  free(inner_res.pos);
}

/// Nested Loop join
///
/// Breaks the outer side into `tpool.thread_ct` chunks. Each thread joins
/// its chunk of outer against all of inner and copies results when finished.
void pr_join_nested_loop(JoinInput outer, JoinInput inner, ThreadPool *tpool) {
  assert(outer.val->len >= inner.val->len);
  assert(outer.res->pos == NULL);
  assert(inner.res->pos == NULL);
  if (outer.val->len == 0 || inner.val->len == 0) {
    return;
  }

  LockedJoinRes ljr = ljr_new(outer.res, inner.res);
  Channel finished = chan_new(tpool->thread_ct);

  size_t outer_per_task = ceil((double)outer.pos->len / tpool->thread_ct);
  JoinInnerArgs args[tpool->thread_ct];
  TaskInput tasks[tpool->thread_ct];

  size_t task_ct = 0;
  for (size_t task_idx = 0; task_idx < tpool->thread_ct; task_idx++) {
    size_t outer_idx = task_idx * outer_per_task;
    if (outer_idx >= outer.val->len) {
      break;
    }
    task_ct++;
    size_t outer_len = min_unsig(outer_per_task, outer.val->len - outer_idx);
    args[task_idx] = (JoinInnerArgs){.ljr = &ljr,
                                     .outer = &outer,
                                     .inner = &inner,
                                     .outer_start = outer_idx,
                                     .outer_len = outer_len,
                                     .finished = &finished};
    tasks[task_idx] =
        (TaskInput){.task = pr_join_inner_loop, .arg = &args[task_idx]};
    tpool_task_run(tpool, &tasks[task_idx]);
  }

  for (size_t ct = 0; ct < task_ct; ct++) {
    chan_recv(&finished);
  }
}

/// Treat this as moving the vector. Don't use it again after this.
inline ChandlePosList pr_vec_to_poslist(Vec vec) {
  return (ChandlePosList){.pos = (uint64_t *)vec.arr, .len = vec.len};
}

#define DEFAULT_HASH_RES_CT 256

/// Single-pass Hash Joins
///
/// Loads inner into a hash table and joins outer against it.
/// Used in small hash joins and by each worker thread in grace joins.
///
/// Uses a less optimal vector-based strategy that accomodates larger input
/// sizes.
void pr_join_single_hash_large(JoinInput outer, JoinInput inner) {
  HashTable ht = htbl_new(inner.val->len);
  for (size_t idx = 0; idx < inner.val->len; idx++) {
    htbl_put(&ht, inner.val->nums[idx], (Generic){.uint = inner.pos->pos[idx]});
  }

  Vec res_outer = vec_new(DEFAULT_HASH_RES_CT);
  Vec res_inner = vec_new(DEFAULT_HASH_RES_CT);
  Vec buf = vec_new(8);
  assert(sizeof(uint64_t) == sizeof(Generic));
  for (size_t idx = 0; idx < outer.val->len; idx++) {
    int key = outer.val->nums[idx];
    htbl_get(&ht, key, &buf);

    for (size_t got = 0; got < buf.len; got++) {
      vec_push(&res_inner, buf.arr[got]);
      vec_push(&res_outer, (Generic){.uint = outer.pos->pos[idx]});
    }
    vec_clear(&buf);
  }

  assert(outer.res->pos == NULL);
  assert(inner.res->pos == NULL);

  // sorting outputs is slow - took that out
  *outer.res = pr_vec_to_poslist(res_outer);
  *inner.res = pr_vec_to_poslist(res_inner);

  // Don't free the vecs, they were simply moved
  vec_free(&buf);
  htbl_free(&ht);
}

/// Another single-pass join implementation, but this one uses a
/// faster but less space approach that's faster. Use it anywhere
/// that's guaranteed smaller than the grace cutoff.
void pr_join_single_hash_small(JoinInput outer, JoinInput inner) {
  HashTable ht = htbl_new(inner.val->len);
  for (size_t idx = 0; idx < inner.val->len; idx++) {
    htbl_put(&ht, inner.val->nums[idx], (Generic){.uint = inner.pos->pos[idx]});
  }

  size_t max_len = outer.pos->len * inner.pos->len;
  size_t res_len = 0;
  uint64_t *res_outer = MALLOC(sizeof(uint64_t) * max_len);
  uint64_t *res_inner = MALLOC(sizeof(uint64_t) * max_len);

  Vec buf = vec_new(8);
  assert(sizeof(uint64_t) == sizeof(Generic));
  for (size_t idx = 0; idx < outer.val->len; idx++) {
    int key = outer.val->nums[idx];
    htbl_get(&ht, key, &buf);

    for (size_t got = 0; got < buf.len; got++) {
      res_inner[res_len] = buf.arr[got].uint;
      res_outer[res_len++] = outer.pos->pos[idx];
    }
    vec_clear(&buf);
  }

  assert(outer.res->pos == NULL);
  assert(inner.res->pos == NULL);

  res_outer = REALLOC(res_outer, sizeof(uint64_t) * res_len);
  res_inner = REALLOC(res_inner, sizeof(uint64_t) * res_len);

  // sorting outputs is slow - took that out
  *outer.res = (ChandlePosList){.pos = res_outer, .len = res_len};
  *inner.res = (ChandlePosList){.pos = res_inner, .len = res_len};

  vec_free(&buf);
  htbl_free(&ht);
}

// Each partition has this many kv pairs, which together
// use about 4096 * 512 bytes.
// This size locally outperformed 4096 * 1024 and 4096 * 256.
#define GRACE_DISK_PARTITION_LEN 174762
typedef struct {
  int keys[GRACE_DISK_PARTITION_LEN];
  uint64_t pos[GRACE_DISK_PARTITION_LEN];
} GraceDiskPartition;

// Each buffer takes up 1024 bytes
#define GRACE_BUF_LEN 83
typedef struct {
  size_t curr_len;
  size_t disk_len;
  size_t split;
  int keys[GRACE_BUF_LEN];
  uint64_t pos[GRACE_BUF_LEN];
} GraceBuf;

void pr_flush_grace_buf(GraceBuf *buf, GraceDiskPartition *part) {
  if (buf->curr_len == 0) {
    return;
  }
  if (buf->disk_len + buf->curr_len > GRACE_DISK_PARTITION_LEN) {
    log_err("Attempted to overfill a grace disk partition");
    exit(1);
  }
  memcpy(&part->keys[buf->disk_len], buf->keys, sizeof(int) * buf->curr_len);
  memcpy(&part->pos[buf->disk_len], buf->pos, sizeof(uint64_t) * buf->curr_len);
  buf->disk_len += buf->curr_len;
  buf->curr_len = 0;
}

void pr_fill_grace_partitions(JoinInput input, GraceDiskPartition *parts,
                              GraceBuf *bufs, size_t part_ct) {
  for (size_t idx = 0; idx < input.val->len; idx++) {
    int key = input.val->nums[idx];
    uint64_t pos = input.pos->pos[idx];

    uint64_t hash = htbl_hash(key, 64) % part_ct;
    GraceBuf *buf = &bufs[hash];

    buf->keys[buf->curr_len] = key;
    buf->pos[buf->curr_len++] = pos;
    if (buf->curr_len == GRACE_BUF_LEN) {
      pr_flush_grace_buf(buf, &parts[hash]);
    }
  }

  for (size_t idx = 0; idx < part_ct; idx++) {
    pr_flush_grace_buf(&bufs[idx], &parts[idx]);
  }
}

/// Inputs passed to a partition grace join task
typedef struct {
  LockedJoinRes *ljr;
  GraceDiskPartition *part;
  Channel *finished;
  size_t len;
  size_t split_idx;
} JoinPartitionArgs;

void pr_join_grace_partition(void *join_partition_args) {
  JoinPartitionArgs *args = join_partition_args;

  size_t inner_len = args->split_idx;
  size_t outer_len = args->len - args->split_idx;

  if (inner_len == 0 || outer_len == 0) {
    return;
  }

  ChandleIntVec temp_key_outer = {.nums = &args->part->keys[args->split_idx],
                                  .len = outer_len};
  ChandlePosList temp_pos_outer = {.pos = &args->part->pos[args->split_idx],
                                   .len = outer_len};
  ChandlePosList temp_res_outer = {.pos = NULL, .len = 0};

  ChandleIntVec temp_key_inner = {.nums = args->part->keys, .len = inner_len};
  ChandlePosList temp_pos_inner = {.pos = args->part->pos, .len = inner_len};
  ChandlePosList temp_res_inner = {.pos = NULL, .len = 0};

  pr_join_single_hash_small((JoinInput){.val = &temp_key_outer,
                                        .pos = &temp_pos_outer,
                                        .res = &temp_res_outer},
                            (JoinInput){.val = &temp_key_inner,
                                        .pos = &temp_pos_inner,
                                        .res = &temp_res_inner});

  ljr_lock(args->ljr);
  pr_join_copy_results(args->ljr->res_outer, &temp_res_outer);
  pr_join_copy_results(args->ljr->res_inner, &temp_res_inner);
  ljr_unlock(args->ljr);
  chan_send(args->finished, (Generic){.uint = 0});

  free(temp_res_outer.pos);
  free(temp_res_inner.pos);
}

/// Grace Hash joins
///
/// Joins outer and inner. Uses disk partitioning, so this will succeed as long
/// as the final result fits in memory. Parallelizes partition joins across
/// available cores and uses single-pass hash joins within each partition.
void pr_join_grace_hash(JoinInput outer, JoinInput inner, ThreadPool *tpool) {
  // Underfill the partitions to accomodate hash variance
  size_t part_ct =
      ceil(((outer.pos->len + inner.pos->len) * 2.) / GRACE_DISK_PARTITION_LEN);

  char *fpath = "/tmp/db_grace_join";
  int fd = open(fpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("open");
    exit(1);
  }

  size_t file_bytes = sizeof(GraceDiskPartition) * part_ct;
  if (ftruncate(fd, file_bytes) < 0) {
    perror("ftruncate");
    exit(1);
  }

  GraceDiskPartition *parts =
      mmap(NULL, file_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (close(fd) < 0) {
    perror("close");
    exit(1);
  }
  GraceBuf *fill_bufs = MALLOC(sizeof(GraceBuf) * part_ct);

  for (size_t idx = 0; idx < part_ct; idx++) {
    fill_bufs[idx] = (GraceBuf){.curr_len = 0, .disk_len = 0, .split = 0};
  }

  pr_fill_grace_partitions(inner, parts, fill_bufs, part_ct);
  for (size_t idx = 0; idx < part_ct; idx++) {
    fill_bufs[idx].split = fill_bufs[idx].disk_len;
  }
  pr_fill_grace_partitions(outer, parts, fill_bufs, part_ct);

  size_t output = 0;
  for (size_t idx = 0; idx < part_ct; idx++) {
    output += fill_bufs[idx].disk_len;
  }

  LockedJoinRes ljr = ljr_new(outer.res, inner.res);
  Channel finished = chan_new(part_ct);
  JoinPartitionArgs args[part_ct];
  TaskInput tasks[part_ct];

  for (size_t idx = 0; idx < part_ct; idx++) {
    args[idx] = (JoinPartitionArgs){.ljr = &ljr,
                                    .part = &parts[idx],
                                    .finished = &finished,
                                    .len = fill_bufs[idx].disk_len,
                                    .split_idx = fill_bufs[idx].split};
    tasks[idx] =
        (TaskInput){.arg = &args[idx], .task = pr_join_grace_partition};
    tpool_task_run(tpool, &tasks[idx]);
  }

  for (size_t ct = 0; ct < part_ct; ct++) {
    chan_recv(&finished);
  }

  ljr_free(&ljr);
  chan_free(&finished);
  free(fill_bufs);

  if (munmap(parts, file_bytes) < 0) {
    perror("munmap");
    exit(1);
  }
}

// In-memory if there's space. Grace hash if disk is needed.
void pr_join_decide_hash(JoinInput outer, JoinInput inner, ThreadPool *tpool) {
  // Use grace if parallelization will be useful (also a proxy for size).
  size_t use_grace =
      (outer.pos->len + inner.pos->len) >= GRACE_DISK_PARTITION_LEN;
  if (use_grace) {
    pr_join_grace_hash(outer, inner, tpool);
  } else {
    pr_join_single_hash_small(outer, inner);
  }
}

JoinResult db_join(ClientCxt2 *cxt, ParseJoin *req, ThreadPool *tpool) {
  Chandle *fet1 = chandle_get(cxt, req->fet_chname1);
  Chandle *sel1 = chandle_get(cxt, req->sel_chname1);
  Chandle *fet2 = chandle_get(cxt, req->fet_chname2);
  Chandle *sel2 = chandle_get(cxt, req->sel_chname2);

  if (!fet1 || !sel1 || !fet2 || !sel2) {
    return (JoinResult){.status = status_err("join: chname not found\n")};
  }
  if (fet1->type != CHANDLE_INTVEC || fet2->type != CHANDLE_INTVEC) {
    return (JoinResult){.status =
                            status_err("join: f1 and f2 must be intvecs\n")};
  }

  JoinInput outer;
  JoinInput inner;

  outer.val = &fet1->val.iv;
  inner.val = &fet2->val.iv;

  chandle_try_poslist(sel1);
  chandle_try_poslist(sel2);
  if (sel1->type != CHANDLE_POSLIST || sel2->type != CHANDLE_POSLIST) {
    return (JoinResult){
        .status = status_err("join: s1 and s2 must be poslist convertible\n")};
  }

  outer.pos = &sel1->val.pos;
  inner.pos = &sel2->val.pos;

  assert(outer.val->len == outer.pos->len);
  assert(inner.val->len == inner.pos->len);
  assert(outer.val->len > 0);
  assert(inner.val->len > 0);

  JoinResult res = (JoinResult){.status = status_ok(),
                                .pos1 = {.pos = NULL, .len = 0},
                                .pos2 = {.pos = NULL, .len = 0}};
  outer.res = &res.pos1;
  inner.res = &res.pos2;

  // Ensure outer is not shorter than inner
  if (outer.val->len < inner.val->len) {
    // Swap
    JoinInput outer_temp;
    memcpy(&outer_temp, &outer, sizeof(JoinInput));

    memcpy(&outer, &inner, sizeof(JoinInput));
    memcpy(&inner, &outer_temp, sizeof(JoinInput));
  }
  assert(outer.val->len >= inner.val->len);

  switch (req->jtype) {
  case JOIN_NESTED: {
    pr_join_nested_loop(outer, inner, tpool);
    return res;
  }
  case JOIN_SINGLE_HASH: {
    pr_join_single_hash_large(outer, inner);
    return res;
  }
  case JOIN_GRACE_HASH: {
    pr_join_grace_hash(outer, inner, tpool);
    return res;
  }
  case JOIN_DECIDE_HASH: {
    pr_join_decide_hash(outer, inner, tpool);
    return res;
  }
  }
  log_err("Missed case in join\n");
  exit(1);
}
