#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace my_std {

class thread_pool {
private:
    bool stop_;
    std::mutex m_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> task_queue_;
    std::vector<std::jthread> workers_;

    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock{m_};
                cv_.wait(lock, [this]{ return stop_ || !task_queue_.empty(); });

                if (stop_ && task_queue_.empty()) return;

                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
            task();
        }
    }

public:
    thread_pool(std::size_t n) {
        workers_.reserve(n);
        try {
            for (std::size_t i = 0; i < n; i++) {
                workers_.emplace_back([this]{ worker_loop(); });
            }
        } catch(...) {
            shut_down();
            throw;
        }
    }

    template <typename Fn, typename...Args>
    auto submit(Fn&& fn, Args&&... args) -> std::future<std::invoke_result_t<std::decay_t<Fn>, std::decay_t<Args>...>> {
        using R = std::invoke_result_t<std::decay_t<Fn>, std::decay_t<Args>...>;

        auto task = std::make_shared<std::packaged_task<R()>>(
            [fn = std::decay_t<Fn>, ...args = std::decay_t<Args>]() mutable -> R { std::invoke(std::move<Fn>(fn), std::move(args)...);}
        );

        std::future<R> f = task->get_future();

        {
            std::lock_guard lock{m_};
            task_queue_.emplace([task] { (*task)(); });
        }

        return f;
    }

    void shut_down() {
        std::lock_guard lock{m_};
        stop_ = true;
    }
};

}