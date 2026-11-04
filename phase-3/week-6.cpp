// g++ -std=c++20 -pthread phase-3/week-6.cpp -o phase-3/week-6.out && ./phase-3/week-6.out

// Includes the entire std library for C++
#include <bits/stdc++.h>
using namespace std;

class BlockingQueue {
private:
    queue<int> q;
    mutex mtx;
    condition_variable cv;
    bool closed = false;

public:
    void Push(int value) {{
            lock_guard<mutex> lock(mtx);
            if (closed) return;
            q.push(value);
        }
    
        cv.notify_one();
    }

    optional<int> Pop() {
        unique_lock<mutex> lock(mtx);

        cv.wait(lock, [this] {return !q.empty() || closed;});

        if (q.empty()) {return nullopt;}

        int value = q.front();
        q.pop();

        return value;
    }

    void Close() {
        lock_guard<mutex> lock(mtx);
        closed = true;
        
        cv.notify_all();
    }
};

int main() {
    BlockingQueue q;

    int count = 0;
    int sum = 0;
    bool inOrder = true;

    // Start consumer first
    thread consumer([&]() {
        int expected = 0;

        while (auto value = q.Pop()) {
            cout << "Consumed: " << *value << " ";
            if (*value != expected) {inOrder = false;}
            expected++;
            count++;
            sum += *value;
        }
    });

    // Wait 100 ms before starting producer
    this_thread::sleep_for(chrono::milliseconds(100));

    // Producer
    thread producer([&]() {
        for (int i = 0; i <= 100; i++) {
            q.Push(i);
            this_thread::sleep_for(chrono::milliseconds(1));
        }

        q.Close();
    });

    // Wait for both threads
    producer.join();
    consumer.join();

    cout << "\nTotal count: " << count << endl;
    cout << "Total sum: " << sum << endl;
    cout << "In order: " << (inOrder ? "Yes" : "No") << endl;

    return 0;
}
