
#pragma once

#include <mutex>
#include <queue>
#include <vector>

namespace xqueue
{

template <typename T> struct Queue
{
        std::mutex    mutex;
        std::queue<T> queue;

        Queue() : mutex(std::mutex()), queue(std::queue<T>()) {}

        bool empty()
        {
                this->mutex.lock();

                auto r = this->queue.empty();

                this->mutex.unlock();

                return r;
        }

        void push(T&& t)
        {
                this->mutex.lock();

                this->queue.push(std::move(t));

                this->mutex.unlock();
        }

        void push_range(std::vector<T> list)
        {
                this->mutex.lock();

                this->queue.push_range(std::move(list));

                this->mutex.unlock();
        }

        T pop()
        {
                this->mutex.lock();

                T t = std::move(this->queue.front());
                this->queue.pop();

                this->mutex.unlock();

                return t;
        }
};

} // namespace xqueue
