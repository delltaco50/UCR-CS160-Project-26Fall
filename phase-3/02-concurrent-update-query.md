# Phase 3b: Concurrent Update and Query
## Introduction

With the Phase 3a update pipeline, a straightforward way to answer an SSSP query is to wait until the trace finishes and all updates have been applied. Phase 3b adds **SSSP queries** to the same stream: the trace now interleaves `ADD src dst weight` records with `Q` records, and queries must run **concurrently** with ongoing updates.

The hard part is determinism. If a query reads a vertex's adjacency while an updater appends a new edge to it, the answer would depend on thread scheduling. We make the output reproducible by assigning every record a monotonically increasing **version** at the producer, and tagging each edge with the version of the ADD that produced it. A query at version `q` is defined to see exactly the graph state after every request with version `< q` has been applied: it skips any edge whose version is `>= q`.

Because the graph is add-only, SSSP distances can only decrease over time. The query path exploits this by **caching** the distance array between queries and only re-relaxing from vertices whose out-edges have changed: an **incremental SSSP**. The relaxation itself reuses your [Phase 2 push-style parallel BSP](../phase-2/03-atomic-worklists.md).

## Requirements

Two thread-count parameters appear below: `TC` is the number of updater threads draining the update queue, and `TQ` is the number of BSP worker threads used inside each SSSP run.

1. **Versioned edges.** Each edge carries the version of the ADD that produced it:

   ```cpp
   struct Edge { uint32_t dst; int weight; uint64_t version; };
   ```

   Edges loaded from `initial.txt` use version `0`. Records from `updates.txt` are versioned `1, 2, 3, ...` by the producer in arrival order. Both ADD and Q consume version slots; D records do not.

2. **Two queues.** Reuse `ConcurrentQueue<T>` from [Phase 3a](01-graph-update.md#concurrent-queue), instantiated twice: `update_queue` is drained by `TC` updater threads, and `query_queue` is drained by **one** query thread.

3. **Adjacency storage.** Adjacency is stored as a `std::vector<Edge>` per source vertex, paired with a `std::shared_mutex` per source vertex. Updaters take `unique_lock` to append; the SSSP scan takes `shared_lock`.

4. **Version-applied gate.** A query at version `q` must not start scanning the graph until every ADD with `version < q` has been applied.

5. **Incremental SSSP.** `dist[]` and its previous-round snapshot `dist_prev[]` are allocated once and reused across queries. Each query runs the Phase 2 push-style parallel BSP with `TQ` worker threads, reading source distances from `dist_prev[]`, scanning each vertex's out-edges under a shared lock, and skipping any edge whose `version >= q`. Destination distances in `dist[]` are updated atomically.

6. **Output.** For each query at version `q`, append one line `<q> <signature(dist)>` to `queries.out`. The signature is defined in *Testing & Verification*.

## Hints

### Trace format

`updates.txt` contains three kinds of records:

```
ADD <src> <dst> <weight>
Q
D <ms>
```

The query source vertex is fixed to `0` throughout this phase, so `Q` carries no payload. `D` is the slow-producer delay from Phase 3a and is not enqueued.

### Per-vertex shared mutex

`std::shared_mutex` lets multiple readers hold the lock in parallel without blocking each other. An updater appending an edge from that vertex takes the lock in exclusive mode and waits for all readers to release.

### Producer loop and `pending_dirty`

For each query, the BSP needs an initial frontier of vertices whose out-edges may have changed since the previous query, i.e., the source vertices of every ADD with `last_q_version < version < q_version`. The producer collects this set as it dispatches: a thread-local `pending_dirty` vector receives the `src` of every ADD. At each `Q`, the vector is moved into the outgoing `QueryReq` and a fresh empty vector takes its place. Seed `pending_dirty` with `{source}` so the very first query's frontier already contains the SSSP source.

Pre-parsing of `updates.txt` into an in-memory record list is done before timing starts (same convention as Phase 3a).

```
ver = 0
pending_dirty = {source}
for each record:
  case ADD(src, dst, w):
    ++ver
    update_queue.Push({src, dst, w, ver})
    pending_dirty.push_back(src)
  case Q:
    ++ver
    query_queue.Push({ver, std::move(pending_dirty)})
    pending_dirty.clear()
  case D(ms):
    sleep_for(ms)
update_queue.Close();
query_queue.Close();
```

### Updater loop

```
while (auto req = update_queue.Pop()) {
  {
    unique_lock lk(locks[req->src]);
    adj[req->src].push_back({req->dst, req->weight, req->version});
  }
  apply_tracker.MarkDone(req->version);
}
```

### `ApplyTracker`

Updater threads can finish ADDs out of version order: thread A may dequeue version 5 and finish before thread B finishes version 3. The query thread needs a barrier that does not fire until every version below `q` is applied, regardless of completion order. `ApplyTracker` is that barrier.

It exposes two operations:
- `MarkDone(v)`: an updater calls this after committing the ADD at version `v`.
- `WaitUntil(target)`: the query thread blocks until `applied_max >= target`.

Out-of-order completions are buffered in a small set; `applied_max` advances only as the gap immediately above it closes.

```
struct ApplyTracker {
  mutex mu;
  condition_variable cv;
  uint64_t applied_max = 0;
  set<uint64_t> done_above;

  MarkDone(v):
    lock(mu)
    if v == applied_max + 1:
      // advance applied_max past any contiguous versions
      // buffered in done_above, then notify waiters
    else:
      done_above.insert(v)

  WaitUntil(target):
    // block until applied_max >= target
}
```

### Query thread

A single query thread manages `dist[]` and `dist_prev[]` across queries and orchestrates the BSP for each one.

```
dist.assign(n, INF); dist[source] = 0
dist_prev = dist
while (auto req = query_queue.Pop()) {
  apply_tracker.WaitUntil(req->version - 1)

  sort(req->dirty_sources); unique(...)   // dedup in place
  RunBspSsspParallel(adj, locks, dist, dist_prev,
                     req->dirty_sources,  // initial frontier
                     req->version,        // edge-version cutoff
                     /*nthreads=*/TQ)

  append_result(req->version, signature(dist))
  apply_tracker.MarkDone(req->version)
}
```

The persistent distance arrays make this incremental: the second query starts from the first query's result, not from `INF`.

### Inside the parallel BSP step

Each round, partition the current frontier across the `TQ` worker threads. For each `u` in your slice:

```
du = dist_prev[u]
if du == INF: skip u
shared_lock lk(locks[u])
for (Edge e : adj[u]) {
  if (e.version >= q_version) continue
  // Phase 2c atomic relaxation on dist[e.dst] with du + e.weight
}
```

Keep `dist_prev[]` unchanged while workers run. After all workers have joined and the deduplicated next frontier has been formed, copy `dist_prev[v] = dist[v]` for each vertex in that next frontier, as in Phase 2c. Finish these snapshot updates before starting the next round.

The `e.version >= q_version` filter is what makes concurrent ADDs at version `> q` invisible to this query, even if they have already been published into the adjacency vector by the time you read it.

### Measurement window

Time only the producer-consumer phase, as in Phase 3a. The initial graph load and the `updates.txt` pre-parse are not counted. Spawn the `TC` updater threads and the one query thread before starting the clock so they are parked on their respective queues when timing begins.

Start the clock immediately before the producer begins processing the in-memory records, and stop it after all updater threads and the query thread have joined. Include query execution and signature computation in this window. Buffer query results in memory during the run, then write `queries.out` after stopping the clock; file output is not timed.

## Testing & Verification

Download `initial.txt`, `updates.txt`, and `expected_queries.txt` for Phase 3b from the [shared folder](https://drive.google.com/drive/folders/1JqrwU5KdN1eWETsQaT_AdW4DzNJr0BhQ?usp=drive_link). `updates.txt` is regenerated for this phase with `Q` records interleaved at random positions.

**Signature.** For each query, define

```
S = (sum over v of (dist[v] if dist[v] != INF else 0)) mod (10^9 + 7)
```

and write one line per query to `queries.out`: `<version> <S>`.

**Baseline.** A single-threaded reference run that consumes the trace in version order, applies each ADD synchronously, and runs a full SSSP at every `Q`, produces `expected_queries.txt`. Your concurrent implementation, for any `TC` and `TQ` and any thread schedule, must produce a `queries.out` that is identical to `expected_queries.txt`.

## Experiments

Implement two SSSP variants:

- **incremental**: the design described above. `dist[]` and `dist_prev[]` are persistent across queries; the initial frontier is the deduplicated `pending_dirty` from the `QueryReq`.
- **full**: at every `Q`, reset both distance arrays to `INF`, set the source distance to 0 in both, and run BSP from `{source}`. The source vertex is 0. Everything else (version filter, per-vertex locks, `ApplyTracker`) is unchanged.

Both modes must produce identical `queries.out`. Run each on the provided trace with `TC = 2` and `TQ = 2`, and report the timed wall time in your project report:

| Mode | `TC` | `TQ` | Wall time (s) |
| --- | ---: | ---: | ---: |
| Full | 2 | 2 | |
| Incremental | 2 | 2 | |

### Discussion

Answer the following in your project report:

1. Compare the times. Which is faster? Why? You can measure each query separately to get a detailed picture.
2. Skip the `WaitUntil` call in the query thread. Does the output still match `expected_queries.txt`? Do repeated runs produce the same numbers? Explain what you observe.
3. Try different `TC` / `TQ` combinations on the incremental version and report any findings: where does the time go, what looks like a bottleneck, and any ideas you have for improving the performance.
