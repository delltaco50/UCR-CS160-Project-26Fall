// Build (GCC or LLVM Clang): clang++ -std=c++17 -fopenmp openmp.cpp -o openmp
// Run: ./openmp

#include <cstdio>
#include <omp.h>

int main() {
    int nthreads = 4;

    #pragma omp parallel for num_threads(nthreads) schedule(static)
    for (int i = 0; i < 16; ++i) {
        std::printf("Iteration %2d: thread %d\n", i, omp_get_thread_num());
    }
}
