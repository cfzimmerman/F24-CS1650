#ifndef CZ_THREADPOOL
#define CZ_THREADPOOL

#include "channel.h"
#include <stdatomic.h>

typedef struct TaskInput {
  /// A function for the thread to execute.
  void (*task)(void *arg);

  /// The input for this task to operate on. The caller is
  /// responsible for ensuring the pointer is safe to access for
  /// the duration of the task.
  void *arg;
} TaskInput;

/// A fixed-size threadpool. A given number of threads is initialized
/// at startup. Each thread waits for tasks, completes them, and blocks
/// until it receives more tasks to complete.
typedef struct ThreadPool {
  Channel task_queue; // Channel<TaskInput>
  pthread_t *threads;
  size_t thread_ct;
  atomic_bool stop;
} ThreadPool;

/// Initializes a threadpool with a fixed number of threads.
///
/// The returned pool is heap allocated and should only be deallocated
/// using `tpool_free`.
ThreadPool *tpool_new(size_t thread_ct);

/// Cleans up all resources held by the threadpool. This function blocks until
/// current tasks are finished.
void tpool_free(ThreadPool *pool);

/// Runs a task in the pool. The caller must ensure that `input` is safe
/// to dereference at least as long as the task might be running.
///
/// If the task queue is currently full, this function blocks until a worker
/// has picked up the task. The pool oversizes its worker queue to minimize the
/// chance of this happening.
void tpool_task_run(ThreadPool *pool, TaskInput *input);

/// Returns a reasonable guess for the number of threads that should
/// be in the pool based on the number of machine cores.
size_t tpool_suggest_size();

#endif
