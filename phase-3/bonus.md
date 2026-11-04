# Bonus: Incremental SSSP with Edge Deletions

[Phase 3 overview](README.md) · [Concurrent Update and Query](02-concurrent-update-query.md)

This optional task is worth **3 additional points**. Submit a separate bonus report documenting your design and performance analysis, and present your solution in person as described below.

## Introduction and Requirements

Phase 3b's incremental design leans on a key property: the graph is add-only, so SSSP distances are monotonically non-increasing, and the initial frontier for each query is simply the source vertices of new edges. The bonus extends the trace with a new record type:

```
DEL <src> <dst>
```

which removes an existing edge. `DEL src dst` removes all existing edges from `src` to `dst`, regardless of weight. DEL gets a version slot just like ADD and Q. Your implementation must continue to produce a `queries.out` identical to a single-threaded baseline that processes the trace in version order. The hard part is incremental SSSP under deletion: removing an edge `(u, v, w)` can *increase* `dist[v]` if `v`'s previous shortest path went through that edge, which the add-only frontier construction cannot detect. You will need to rethink what the frontier is, what invariants the cached `dist[]` carries across queries, and how to bound the work per query so the incremental version still beats a from-scratch SSSP.

If you want to attempt this, **finish the main Phase 3b task first**, then contact the TA to discuss your proposed approach **before** you start coding. We will go over the algorithm together; once we agree on a plan, implement it and present your solution in person during the lab section or office hours. The TA may ask about the details of your algorithm and your performance analysis, similar to what you have done in your report.
