# Phase 3a: Graph Update
## Introduction

So far the graph has been static: load once, run an algorithm, exit. Real systems receive a stream of edge updates while queries are still running. Phase 3a builds the smallest version of that pipeline: a single **producer** reads update requests from a trace file, and `N` **consumer** threads apply them to an in-memory graph, communicating through a **concurrent queue** that you will implement.

The graph in this phase is stored as out-neighbor adjacency lists. Define a small record for each out-edge:

```cpp
struct Edge { uint32_t dst; int weight; };
```

and store the graph as `std::vector<std::vector<Edge>>`. Applying one ADD is just a `push_back` on row `src`. Two consumers updating different rows do not collide; two consumers updating the same row do. Each source row therefore carries its own mutex.

## Requirements

Implement three pieces and wire them together.

1. **Concurrent queue.** A blocking, unbounded queue with `Push(T)`, `Pop() -> std::optional<T>`, and `Close()`. Use one `std::mutex` plus one `std::condition_variable`. `Pop` blocks while the queue is empty and open; once `Close()` has been called and the queue is drained, `Pop` returns `std::nullopt` so each consumer can exit cleanly.

2. **Graph and apply path.** Hold the adjacency list as `std::vector<std::vector<Edge>>` plus a parallel `std::vector<std::mutex>`, one mutex per source vertex. Applying an ADD: lock `locks[src]`, `push_back({dst, weight})`, unlock.

3. **Producer + consumers.** First, **pre-load** `initial.txt` into the adjacency list and **pre-parse** `updates.txt` into memory: an in-order list of ADD records and a parallel list of `D` markers (each marker remembers its position in the ADD stream and the sleep duration). Both happen *before* timing starts and are not counted toward the measurement.

   Then spawn `N` consumer threads that loop on `Pop` and apply each request, and have the single producer iterate the in-memory lists. Two request shapes:
   - `ADD <src> <dst> <weight>`: push to the queue.
   - `D <ms>`: the producer itself sleeps for `ms` milliseconds before continuing. **`D` is not enqueued and not consumed.** It models a slow producer.

   When the producer reaches the end of the in-memory list, it calls `Close()`, then joins the consumers.

**Correctness.** After every consumer has joined, the per-vertex weight sums computed from the in-memory adjacency list, summed over all vertices and reduced mod `10^9 + 7`, must equal the same quantity computed from the original full edge list. See *Testing & Verification*.

## Hints

### Concurrent queue

Two things worth noticing as you design the queue:

- `Pop` waits on the condition variable with a predicate that checks **both** "queue is non-empty" and "queue is closed". Waiting only on `!empty` deadlocks: if the producer closes the queue while a consumer is parked, that consumer never wakes and never returns end-of-stream.
- A successful `Push` only needs to wake one waiter (`notify_one`). `Close()` must wake every waiter (`notify_all`), because each parked consumer needs to observe the closed state and return `nullopt` independently.

### Producer loop

The producer walks two pre-parsed arrays in lockstep: `adds[]` and `delays[]`, where each `delays[k] = (i, ms)` says "sleep `ms` ms right before pushing `adds[i]`". Sketch:

```
di = 0
for i in [0, adds.size()):
    while di < delays.size() and delays[di].first == i:
        sleep_for(delays[di].second)
        di += 1
    queue.Push(adds[i])
flush any trailing delays past the last ADD
queue.Close()
```

### Consumer loop and termination

```
while (auto req = queue.Pop()) apply(*req);
```

`Pop` returns `nullopt` exactly once per consumer (after `Close()` and the queue is empty), which lets the loop exit. The producer joins all consumers after closing.

### Measurement window

Time **only** the producer-consumer phase. The graph load and the `updates.txt` pre-parse are done first and are *not* counted. Spawn the consumers *before* starting the clock so they are already parked on the empty queue when timing begins; start the clock right before the producer begins iterating `adds[]` / `delays[]`, and stop it after every consumer has joined.

## Testing & Verification

Download `initial.txt` and `updates.txt` from the [shared folder](https://drive.google.com/drive/folders/1H65CH1qnzg-x7N4ZBQGPpADRuLLxjh2a?usp=sharing). The trace was produced by splitting a weighted edge list into an initial / update partition with five `D` entries interleaved at random positions.

**Weight-sum check.** After all updates have been applied, compute

```
S = (sum over v of sum over e in adj[v] of e.weight) mod (10^9 + 7)
```

For the provided dataset, `S` must equal **`834418737`**.

## Experiments

Run the pipeline at `N = 1` and `N = 4` on the same trace, and report the wall time of each in your project report. Follow the [measurement window](#measurement-window) above.

| Consumers (`N`) | Wall time (s) |
| --- | ---: |
| 1 | |
| 4 | |

### Discussion

Answer the following in your project report:

1. Which is faster? Explain the result: where does the time go in the slower version? why?

2. **Optional (recommended):** Based on your analysis in Question 1, propose one change to the queue or its API that would let the four-thread version beat the single-thread baseline. *Hint: which lock is on the hot path of every single request, and how could you take it less often?*

   We encourage you to implement your proposal, then re-run the four-thread pipeline on the same trace, report the performance, and give your analysis.
