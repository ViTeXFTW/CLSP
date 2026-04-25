#include "ThreadPool.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <set>
#include <thread>
#include <vector>

using namespace lsp;
using namespace std::chrono_literals;

TEST(ThreadPool, ZeroWorkersClampsToOne) {
  ThreadPool p(0);
  EXPECT_EQ(p.size(), 1u);
}

TEST(ThreadPool, TasksRunOnPool) {
  ThreadPool p(4);
  std::atomic<int> counter{0};
  for (int i = 0; i < 100; ++i) {
    p.submit([&] { counter.fetch_add(1, std::memory_order_relaxed); });
  }
  p.shutdown();
  EXPECT_EQ(counter.load(), 100);
}

TEST(ThreadPool, MultipleWorkersRunConcurrently) {
  ThreadPool p(4);
  std::atomic<int> inFlight{0};
  std::atomic<int> peak{0};

  for (int i = 0; i < 8; ++i) {
    p.submit([&] {
      int now = inFlight.fetch_add(1) + 1;
      int prev = peak.load();
      while (now > prev && !peak.compare_exchange_weak(prev, now)) {
      }
      std::this_thread::sleep_for(20ms);
      inFlight.fetch_sub(1);
    });
  }
  p.shutdown();
  EXPECT_GE(peak.load(), 2);
}

TEST(ThreadPool, SerialExecutorPreservesOrder) {
  ThreadPool serial(1);
  std::vector<int> seen;
  std::mutex mu;

  for (int i = 0; i < 100; ++i) {
    serial.submit([&, i] {
      std::lock_guard lock(mu);
      seen.push_back(i);
    });
  }
  serial.shutdown();

  ASSERT_EQ(seen.size(), 100u);
  for (size_t i = 0; i < seen.size(); ++i) {
    EXPECT_EQ(seen[i], static_cast<int>(i));
  }
}

TEST(ThreadPool, ShutdownIsIdempotent) {
  ThreadPool p(2);
  p.shutdown();
  p.shutdown(); // must not crash or hang
  p.submit([] {}); // dropped silently
}

TEST(ThreadPool, SubmitAfterShutdownIsDropped) {
  ThreadPool p(2);
  p.shutdown();

  std::atomic<bool> ran{false};
  p.submit([&] { ran = true; });
  std::this_thread::sleep_for(10ms);
  EXPECT_FALSE(ran.load());
}

TEST(ThreadPool, DestructorJoinsWorkers) {
  std::atomic<int> counter{0};
  {
    ThreadPool p(4);
    for (int i = 0; i < 50; ++i) {
      p.submit([&] { counter.fetch_add(1); });
    }
  } // destructor calls shutdown -> joins
  // After dtor, no more increments possible.
  int snapshot = counter.load();
  std::this_thread::sleep_for(10ms);
  EXPECT_EQ(counter.load(), snapshot);
}

TEST(ThreadPool, RunsTasksOnDistinctThreads) {
  ThreadPool p(4);
  std::set<std::thread::id> seen;
  std::mutex mu;
  std::atomic<int> done{0};
  constexpr int N = 16;

  for (int i = 0; i < N; ++i) {
    p.submit([&] {
      std::this_thread::sleep_for(5ms);
      {
        std::lock_guard lock(mu);
        seen.insert(std::this_thread::get_id());
      }
      done.fetch_add(1);
    });
  }
  p.shutdown();
  EXPECT_EQ(done.load(), N);
  EXPECT_GE(seen.size(), 2u);
}
