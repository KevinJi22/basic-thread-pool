#include "threadPool.hpp"
#include "threadsafeQueue.hpp"

#include <iostream>
#include <chrono>

using Clock = std::chrono::steady_clock;

using namespace MyNamespace;

int main() {
    auto t1 = Clock::now();

    std::vector<ThreadPool::TaskFuture<void>> v;
    for (std::uint32_t i = 0u; i < 21u; ++i) {
        v.push_back(DefaultThreadPool::submitJob([]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }));
    }   

    for (auto& item : v) {
        item.get();
    }

    auto t2 = Clock::now();
    std::cout << "Duration: " 
              << std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count()
              << " seconds" << std::endl;

}