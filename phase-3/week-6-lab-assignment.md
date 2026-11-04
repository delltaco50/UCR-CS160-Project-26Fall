This lab implements the blocking concurrent queue from [Phase 3a](01-graph-update.md). You can reuse it in your project.

## Blocking Queue with Close

Implement a queue with the following interface. You may start with `int` values and generalize it to other types later.

```cpp
void Push(int value);
std::optional<int> Pop();
void Close();
```

Use one `std::mutex` and one `std::condition_variable`. See [condition_variable.cpp](condition_variable.cpp) for a usage example.

- `Push()` appends a value and wakes one waiting consumer.
- `Pop()` waits while the queue is empty and open. It removes and returns the front value when available. Once the queue is closed and drained, it returns `std::nullopt`.
- `Close()` marks the queue as closed and wakes all waiting consumers. Consumers must still be able to drain the remaining values.

See the [queue hints in Phase 3a](01-graph-update.md#concurrent-queue) for the wait predicate and notifications.

Start one consumer that repeatedly calls `Pop()` until it returns `std::nullopt`. After a delay of about 100 ms, start one producer that pushes `0, 1, ..., 100` in order, sleeps about 1 ms between pushes, and calls `Close()` after the last push. Join both threads. Check that the consumer receives the values in order, and print the count and sum (expected: **101** and **5050**).

**Optional (recommended):** Try multiple consumers during the lab session. Check that every value is processed exactly once, the combined count and sum are still 101 and 5050, and all consumers exit after the queue is closed and drained. The consumers' printed values need not appear in order.

Submit your queue implementation and producer-consumer test code and a screenshot of the results to Canvas before the lab ends.
