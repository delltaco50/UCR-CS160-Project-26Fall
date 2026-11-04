// Build: c++ -std=c++17 -pthread condition_variable.cpp -o condition_variable
// Run:   ./condition_variable

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

int main() {
    std::mutex mtx;
    std::condition_variable cv;
    bool ready = false;
    int result = 0;

    std::thread consumer([&]() {
        std::unique_lock<std::mutex> lock(mtx);
        // While waiting, release the mutex so the producer can acquire it.
        // Reacquire the mutex before checking the predicate again.
        cv.wait(lock, [&]() { return ready; });
        std::cout << "Result: " << result << '\n';
    });

    // Simulate preparing a result while the consumer may be waiting.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::lock_guard<std::mutex> lock(mtx);
        result = 42;
        ready = true; // Read and write the predicate under the same mutex.
    }
    cv.notify_one(); // Wake a waiting thread; the predicate must still be checked.

    consumer.join();
}
