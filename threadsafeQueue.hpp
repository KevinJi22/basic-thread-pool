/**
 * ThreadSafeQueue class
 * is a wrapper around a basic queue to provide thread safety
 */

#pragma once

#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

namespace MyNamespace {
    template <typename T>
    class ThreadSafeQueue {
        public:
            /**
             * Destructor
             */
            ~ThreadSafeQueue() { invalidate(); }

            /**
             * try to get the first value in the queue
             * returns true if a value was successfully written to out, false otherwise.
             */
            bool tryPop(T& out) {
                std::lock_guard<std::mutex> lock{m_mutex};
                if (m_queue.empty() || !m_valid) {
                    return false;
                }
                out = std::move(m_queue.front());
                m_queue.pop();
                return true;
            }

            /**
             * Get the first value in the queue.
             * Will block until a value is available unless clear is called or the instance is destructed.
             * Returns true if a value was successfully written to the out parameter, false otherwise.
             */
            bool waitPop(T& out) {
                std::unique_lock<std::mutex> lock{m_mutex};
                m_condition.wait(lock, [this]() {
                    return !m_queue.empty() || !m_valid;
                });
                /**
                 * Using condition in predicate ensures that spurious wakeups with a valid but 
                 * empty queue will not proceed, so only need to check for validity before proceeding.
                 */
                if (!m_valid) {
                    return false;
                }
                out = std::move(m_queue.front());
                m_queue.pop();
                return true;
            }

            void push(T value) {
                std::lock_guard<std::mutex> lock{m_mutex};
                m_queue.push(std::move(value));
                m_condition.notify_one();
            }

            bool empty() const {
                std::lock_guard<std::mutex> lock{m_mutex};
                return m_queue.empty();
            }

            void clear() {
                std::lock_guard<std::mutex> lock{m_mutex};
                while (!m_queue.empty()) {
                    m_queue.pop();
                }
                m_condition.notify_all();
            }

            /** invalidate the queue.
             * Ensure that no conditions are being waited on in waitPop
             * when a thread or the application is trying to exit.
             * The queue is invalid after this method and trying to use the queue 
             * after calling this method is an error.
             */
            void invalidate() {
                std::lock_guard<std::mutex> lock{m_mutex};
                m_valid = false;
                m_condition.notify_all();
            }

            bool isValid() const {
                std::lock_guard<std::mutex> lock{m_mutex};
                return m_valid;
            }

        private:
            std::atomic_bool m_valid{true};
            mutable std::mutex m_mutex;
            std::queue<T> m_queue;
            std::condition_variable m_condition;
    };
}

#endif