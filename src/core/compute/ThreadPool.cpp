#include "compute/ThreadPool.h"

#include <algorithm>

namespace resonant::compute {

int hardwareThreads()
{
    return std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
}

int resolveThreads(int requested)
{
    return requested > 0 ? std::min(requested, hardwareThreads()) : hardwareThreads();
}

ThreadPool::ThreadPool(int threads) : threads_(std::max(1, threads))
{
    for (int i = 1; i < threads_; ++i)
        workers_.emplace_back([this, i] { work(i); });
}

ThreadPool::~ThreadPool()
{
    {
        const std::lock_guard lock(mutex_);
        stop_ = true;
        ++generation_;
    }
    start_.notify_all();
    for (std::thread& t : workers_)
        t.join();
}

void ThreadPool::take(const std::function<void(size_t, size_t)>& body, size_t count, size_t chunk)
{
    for (;;) {
        const size_t begin = next_.fetch_add(chunk, std::memory_order_relaxed);
        if (begin >= count)
            return;
        body(begin, std::min(count, begin + chunk));
    }
}

void ThreadPool::run(size_t count, const std::function<void(size_t, size_t)>& body)
{
    if (count == 0)
        return;
    const int parts = static_cast<int>(std::min<size_t>(static_cast<size_t>(threads_), count));
    if (parts == 1) {
        body(0, count);
        return;
    }
    // About eight chunks per thread: small enough to balance unequal cores,
    // large enough that taking one costs nothing next to the work in it.
    const size_t chunk = std::max<size_t>(1, count / (static_cast<size_t>(parts) * 8));
    {
        const std::lock_guard lock(mutex_);
        body_ = &body;
        count_ = count;
        chunk_ = chunk;
        next_.store(0, std::memory_order_relaxed);
        parts_ = parts;
        pending_ = parts - 1;
        ++generation_;
    }
    start_.notify_all();
    take(body, count, chunk);
    std::unique_lock lock(mutex_);
    done_.wait(lock, [this] { return pending_ == 0; });
    body_ = nullptr;
}

void ThreadPool::work(int index)
{
    long seen = 0;
    for (;;) {
        const std::function<void(size_t, size_t)>* body = nullptr;
        size_t count = 0, chunk = 1;
        int parts = 0;
        {
            std::unique_lock lock(mutex_);
            start_.wait(lock, [&] { return generation_ != seen; });
            seen = generation_;
            if (stop_)
                return;
            body = body_;
            count = count_;
            chunk = chunk_;
            parts = parts_;
        }
        if (index < parts) {
            take(*body, count, chunk);
            bool last = false;
            {
                const std::lock_guard lock(mutex_);
                last = --pending_ == 0;
            }
            if (last)
                done_.notify_one();
        }
    }
}

}  // namespace resonant::compute
