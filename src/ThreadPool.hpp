#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace lsp {

/**
 * Fixed-size FIFO thread pool. Submitting after `shutdown()` is a no-op.
 *
 * Construct with `numWorkers == 1` to use as a serial executor: tasks
 * submitted to the same instance are processed strictly in submission order
 * by the single worker.
 */
class ThreadPool {
public:
  explicit ThreadPool(std::size_t numWorkers);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  /**
   * Enqueue a task. Silently dropped if the pool is shutting down.
   */
  void submit(std::function<void()> task);

  /**
   * Stop accepting tasks, wake all workers, and join them. Tasks already
   * dequeued continue to run; queued-but-not-yet-started tasks are
   * abandoned. Idempotent.
   */
  void shutdown();

  std::size_t size() const noexcept { return workers_.size(); }

  /**
   * Default worker count: at least one, at most hardware_concurrency().
   */
  static std::size_t defaultWorkerCount();

private:
  void workerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

} // namespace lsp
