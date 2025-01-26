#include "include/threadpool.h"
#include "include/channel.h"
#include "include/mem.h"
#include <assert.h>
#include <stdatomic.h>
#include <unistd.h>

void do_nothing(void *arg) { (void)arg; }

/// Used to wake up tasks so they can shut themselves down.
const TaskInput DUMMY_TASK = {
    .arg = 0,
    .task = do_nothing,
};

/// Each thread has a worker. Workers pull tasks from the channel,
/// execute them, and then blocking wait for the next task.
/// Tasks exit if the threadpool's stop flag is ever true.
void *pr_tpool_worker(void *tpool) {
  ThreadPool *pool = tpool;
  while (!pool->stop) {
    TaskInput *task = chan_recv(&pool->task_queue).ptr;
    (task->task)(task->arg);
  }
  return 0;
}

/// Initializes a threadpool with a fixed number of threads.
///
/// The returned pool is heap allocated and should only be deallocated
/// using `tpool_free`.
ThreadPool *tpool_new(size_t thread_ct) {
  ThreadPool *pool = MALLOC(sizeof(ThreadPool));
  *pool = (ThreadPool){.stop = ATOMIC_VAR_INIT(false),
                       .thread_ct = thread_ct,
                       .threads = MALLOC(sizeof(pthread_t) * thread_ct),
                       .task_queue = chan_new(thread_ct * 4)};
  // ^ Oversize the task queue to minimize waiting

  for (size_t idx = 0; idx < thread_ct; idx++) {
    if (pthread_create(&pool->threads[idx], NULL, pr_tpool_worker, pool) != 0) {
      perror("THREADPOOL: failed to spawn a new thread");
      exit(1);
    }
  }

  return pool;
}

/// Cleans up all resources held by the threadpool. This function blocks until
/// current tasks are finished.
void tpool_free(ThreadPool *pool) {
  atomic_store(&pool->stop, true);

  // Because stop is now true, every task will exit after running. As long as
  // the queue has at least that many tasks, every task will exit.
  for (size_t idx = 0; idx < pool->thread_ct; idx++) {
    chan_try_send(&pool->task_queue, (Generic){.ptr = (void *)&DUMMY_TASK});
  }

  for (size_t idx = 0; idx < pool->thread_ct; idx++) {
    pthread_join(pool->threads[idx], NULL);
  }

  free(pool->threads);
  chan_free(&pool->task_queue);
  free(pool);
}

/// Runs a task in the pool. The caller must ensure that `input` is safe
/// to dereference at least as long as the task might be running.
///
/// If the task queue is currently full, this function blocks until a worker
/// has picked up the task. The pool oversizes its worker queue to minimize the
/// chance of this happening.
void tpool_task_run(ThreadPool *pool, TaskInput *input) {
  chan_send(&pool->task_queue, (Generic){.ptr = input});
}

/// Returns a reasonable guess for the number of threads that should
/// be in the pool based on the number of machine cores.
size_t tpool_suggest_size() { return sysconf(_SC_NPROCESSORS_ONLN); }
