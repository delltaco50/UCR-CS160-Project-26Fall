# Phase 3: Batched Updates and Incremental Evaluation

Phase 3 combines the following two parts into one three-week project, **November 4–November 24**. Work through them in order and submit your code and reports at the final deadline.

| Part | Description |
| --- | --- |
| [Phase 3a](01-graph-update.md) | Graph Update: a concurrent queue and a producer-consumer update pipeline |
| [Phase 3b](02-concurrent-update-query.md) | Concurrent Update and Query: versioned updates and incremental SSSP |
| [Bonus](bonus.md) | Optional: incremental SSSP with edge deletions |

Each part's document contains its implementation requirements, dataset link, experiments, and Discussion questions. Use the input files from the shared folder linked in that part; 3a and 3b use different traces.

## Submission

Submit code under `phase-3/code/` and **two reports**, one each for 3a and 3b, under `phase-3/report/`. Use the [report template](../REPORT_TEMPLATE.md) for each report, including that part's experiments and Discussion answers. Follow the [project rules](../README.md#project-rules).

For example:

```text
phase-3/report/
├── 3a.md          # or PDF
├── 3b.md
├── bonus.md       # only if attempting the bonus
└── images/
```

If you attempt the bonus, submit a separate bonus report explaining your design, implementation, and experimental results.

**Bonus credit requires both submitting the bonus report and presenting your solution in person.** See the [bonus requirements](bonus.md) for the discussion and presentation arrangements.
