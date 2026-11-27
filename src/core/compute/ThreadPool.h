#pragma once

// A small pool of worker threads for data-parallel loops in the solvers.
//
// run(count, body) splits [0, count) into contiguous chunks that the threads
// (the caller among them) take from a shared counter until none is left, and
// returns when all are done. Taking chunks rather than one fixed share each
// keeps fast cores busy on processors that mix core types. Each index is
// processed by exactly one thread, so a loop whose iterations are
// independent produces bit-identical results for any number of threads and
// any schedule. Reductions must keep their own fixed order (for example, a
// partial sum per index combined sequentially afterwards).
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace resonant::compute {

// The number of threads to use for a request: requested > 0 is taken as
// given (capped at the hardware), 0 means every hardware thread.
int resolveThreads(int requested);
int hardwareThreads();

class ThreadPool {
public:
    explicit ThreadPool(int threads);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    int threads() const { return threads_; }
    // Calls body(begin, end) over a partition of [0, count). Not reentrant.
    void run(size_t count, const std::function<void(size_t, size_t)>& body);

private:
    void work(int index);
    void take(const std::function<void(size_t, size_t)>& body, size_t count, size_t chunk);

    int threads_ = 1;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable start_, done_;
    const std::function<void(size_t, size_t)>* body_ = nullptr;
    size_t count_ = 0;
    size_t chunk_ = 1;
    std::atomic<size_t> next_{0};
    int parts_ = 0;
    long generation_ = 0;
    int pending_ = 0;
    bool stop_ = false;
};

}  // namespace resonant::compute
