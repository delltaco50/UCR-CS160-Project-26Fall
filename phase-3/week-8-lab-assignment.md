This lab implements an **OpenMP-based BSP runner**. Reuse your pull BFS from [Phase 2a](../phase-2/01-pull-bsp.md); keep the `BspAlgorithm` interface and BFS implementation unchanged.

## OpenMP: Static and Dynamic Scheduling

Implement `BspParallelOmp(g, algo, nthreads)` by replacing the runner's `std::thread` scheduling with an OpenMP parallel vertex loop in each round.

- **Static**: use `#pragma omp parallel for num_threads(nthreads) schedule(static)`. Each thread is assigned a contiguous block of vertices.
- **Dynamic**: use `#pragma omp parallel for num_threads(nthreads) schedule(dynamic, chunk)`. Each thread processes a chunk of vertices, then requests another until all vertices have been processed. Choose a positive chunk size and record it with your results.
- Include `<omp.h>` and use `omp_get_thread_num()` inside the loop to obtain `tid` for `Process(tid, v, g)`.

See [openmp.cpp](openmp.cpp) for a minimal static scheduling example.

Compilation examples (replace `main.cpp` with your source files):

```bash
# GCC (or use clang++ for LLVM Clang)
g++ -O2 -std=c++17 -pthread -fopenmp main.cpp -o bfs

# macOS with Homebrew LLVM and libomp installed
"$(brew --prefix llvm)/bin/clang++" -O2 -std=c++17 -pthread -fopenmp main.cpp -o bfs
```

Run **BFS only** on `soc-LiveJournal1-weighted.txt` with source 0 and **4 threads**, using three versions: your Phase 2a `std::thread` runner, OpenMP static, and OpenMP dynamic. Compare each version's distances with the provided `BFS.txt` answers.

Follow the [Phase 2 timing instructions](../phase-2/common.md#overall-experimental-workflow). Print the complete execution times in seconds:

```tex
BFS (std::thread):    time = <seconds>
BFS (OpenMP static):  time = <seconds>
BFS (OpenMP dynamic): time = <seconds>, chunk = <chunk size>
```

Submit your OpenMP runner code and a screenshot of the results to Canvas before the lab ends.
