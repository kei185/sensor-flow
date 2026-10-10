
#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <stop_token>
#include <utility>
#include <vector>

namespace xqueue
{

struct QueueBase
{
        virtual ~QueueBase() = default;
        virtual bool empty() = 0;
};

template <typename T> struct Queue : QueueBase
{
        std::mutex                                   mutex;
        std::unique_ptr<std::condition_variable_any> conditionVariable;
        std::queue<T>                                queue;

        Queue()
            : mutex(std::mutex()),
              conditionVariable(std::make_unique<std::condition_variable_any>()),
              queue(std::queue<T>())
        {}

        bool waitData(std::stop_token st)
        {
                std::unique_lock lock(this->mutex);
                return this->conditionVariable->wait(lock, st, [this] {
                        return !this->queue.empty();
                });
        }

        bool waitData(std::stop_token st, std::chrono::steady_clock::time_point deadline)
        {
                std::unique_lock lock(this->mutex);
                return this->conditionVariable->wait_until(lock, st, deadline, [this] {
                        return !this->queue.empty();
                });
        }

        void awakeConsumer() { conditionVariable->notify_one(); }

        bool empty() override
        {
                std::lock_guard lock(this->mutex);
                return this->queue.empty();
        }

        void push(T&& t)
        {
                std::lock_guard lock(this->mutex);
                this->queue.push(std::move(t));
        }

        void push_range(std::vector<T> list)
        {
                std::lock_guard lock(this->mutex);
                this->queue.push_range(std::move(list));
        }

        T pop()
        {
                std::lock_guard lock(this->mutex);

                T t = std::move(this->queue.front());
                this->queue.pop();

                return t;
        }
};

} // namespace xqueue
