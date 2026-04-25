#include "ThreadPool.hpp"

namespace lsp {

ThreadPool::ThreadPool(std::size_t numWorkers) {
  if (numWorkers == 0) {
    numWorkers = 1;
  }
  workers_.reserve(numWorkers);
  for (std::size_t i = 0; i < numWorkers; ++i) {
    workers_.emplace_back([this] { workerLoop(); });
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

std::size_t ThreadPool::defaultWorkerCount() {
  unsigned hw = std::thread::hardware_concurrency();
  return hw == 0 ? 1u : static_cast<std::size_t>(hw);
}

void ThreadPool::submit(std::function<void()> task) {
  {
    std::lock_guard lock(mu_);
    if (stopping_) {
      return;
    }
    tasks_.push(std::move(task));
  }
  cv_.notify_one();
}

void ThreadPool::shutdown() {
  {
    std::lock_guard lock(mu_);
    if (stopping_) {
      return;
    }
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto& w : workers_) {
    if (w.joinable()) {
      w.join();
    }
  }
}

void ThreadPool::workerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock lock(mu_);
      cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (stopping_ && tasks_.empty()) {
        return;
      }
      task = std::move(tasks_.front());
      tasks_.pop();
    }
    if (task) {
      task();
    }
  }
}

} // namespace lsp
