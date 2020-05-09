/** ThreadPool class.
 * keeps a set of threads constantly waiting to execute incoming jobs.
 */

#pragma once

#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include "ThreadSafeQueue.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace MyNamespace {
    class ThreadPool {
        private:
        class IndThreadTask {
            public:
                IndThreadTask() = default;
                virtual ~IndThreadTask() = default;
                IndThreadTask(const IndThreadTask& rhs) = delete;
                IndThreadTask& operator=(const IndThreadTask& rhs) = delete;
                IndThreadTask(IndThreadTask&& other) = default;
                IndThreadTask& operator=(IndThreadTask&& other) = default;

                virtual void execute() = 0;
        };

        template <typename Func>
        class ThreadTask: public IndThreadTask {
            public:
                ThreadTask(Func&& func):
                    m_func{std::move(func)} {}

                ~ThreadTask() override = default;
                ThreadTask(const ThreadTask& rhs) = delete;
                ThreadTask& operator=(const ThreadTask& rhs) = delete;
                ThreadTask(ThreadTask&& other) = default;
                ThreadTask& operator=(ThreadTask&& other) = default;

                void execute() override {
                    m_func();
                }
            private:
                Func m_func;
        };

    public:
        /** wrapper around std::future that adds behavior of futures returned from std::async.
         * blocks and waits for execution to finish before going out of scope.
         */
        template <typename T>
        class TaskFuture {
            public:
                TaskFuture(std::future<T>&&future):
                    m_future{std::move(future)} {}
                
                TaskFuture(const TaskFuture& rhs) = delete;
                TaskFuture& operator=(const TaskFuture& rhs) = delete;
                TaskFuture(TaskFuture&& other) = default;
                TaskFuture& operator=(TaskFuture&& other) = default;
                ~TaskFuture() {
                    if (m_future.valid()) {
                        m_future.get();
                    }
                }

                auto get() {
                    return m_future.get();
                }

            private:
                std::future<T> m_future;
        };

    public:
        /**
         * Constructor for ThreadPool.
         */
        ThreadPool(void): ThreadPool{std::max(std::thread::hardware_concurrency(), 2u) - 1u}
        // need to always create at least one thread
        {}

        explicit ThreadPool(const std::uint32_t numThreads) :
            m_done{false},
            m_workQueue{},
            m_threads{}
            {
                try {
                    for (std::uint32_t i = 0u; i < numThreads; ++i) {
                        m_threads.emplace_back(&ThreadPool::worker, this);
                    }
                }
                catch(...) {
                    destroy();
                    throw;
                }
            }

            ThreadPool(const ThreadPool& rhs) = delete; // can't copy

            ThreadPool& operator=(const ThreadPool& rhs) = delete; // can't assign

            ~ThreadPool(void) {
                destroy();
            }

            // submit a job to be run
            template <typename Func, typename... Args> 
            auto submit(Func&& func, Args&&... args) {
                auto boundTask = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
                using ResultType = std::result_of_t<decltype(boundTask)()>;
                using PackagedTask = std::packaged_task<ResultType()>;
                using TaskType = ThreadTask<PackagedTask>;

                PackagedTask task{std::move(boundTask)};
                TaskFuture<ResultType> result{task.get_future()};
                m_workQueue.push(std::make_unique<TaskType>(std::move(task)));
                return result;
            }

        private:
            void worker() {
                while (!m_done) {
                    std::unique_ptr<IndThreadTask> pTask{nullptr};
                    if (m_workQueue.waitPop(pTask)) {
                        pTask->execute();
                    }
                }
            }

            // invalidates queue and joins all running threads
            void destroy() {
                m_done = true;
                m_workQueue.invalidate();
                for (auto& thread: m_threads) {
                    if (thread.joinable()) {
                        thread.join();
                    }
                }
            }
            
        private:
            std::atomic_bool m_done;
            ThreadSafeQueue<std::unique_ptr<IndThreadTask>> m_workQueue;
            std::vector<std::thread> m_threads;
    };

    namespace DefaultThreadPool {
        // get default thread pool for application.
        inline ThreadPool& getThreadPool() {
            static ThreadPool defaultPool;
            return defaultPool;
        }

        /** 
         * submit a job to the default thread pool.
         */
        template <typename Func, typename... Args>
        inline auto submitJob(Func&& func, Args&&... args) {
            return getThreadPool().submit(std::forward<Func>(func), std::forward<Args>(args)...);
        }
    }
}

#endif