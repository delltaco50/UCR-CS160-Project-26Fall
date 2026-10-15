// g++ -std=c++20 -pthread phase-1/week-2.cpp -o phase-1/week-2 && ./phase-1/week-2

#include <chrono>
#include <iostream>
#include <thread>
#include <functional>
#include <string>
#include <vector>

struct CSRGraph {
  int num_vertices;
  std::vector<int> offsets;
  std::vector<int> edges;
};

using QueryCallback = std::function<std::string(const CSRGraph&, int src, int K)>;

struct QueryTask {
  int src;
  int K;
  QueryCallback cb;
  std::string result;
};

void worker(int id) {
  for (int i = 0; i < 5; ++i) {
    std::cout << "std::thread worker " << id << ", iteration " << i << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

void RunTasksParallel(const CSRGraph& g, std::vector<QueryTask>& tasks,int num_threads){

  if (tasks.empty()|| num_threads<=0 ){
    return;
  }  

  std::vector<std::thread> workers;
  workers.reserve(num_threads);

  for(int i=0;i<num_threads;i++){
    workers.emplace_back([&, i]() {
      const std::size_t task_count = tasks.size();
      const std::size_t start = task_count * static_cast<std::size_t>(i) /
      static_cast<std::size_t>(num_threads);
      const std::size_t end = task_count * static_cast<std::size_t>(i + 1) /
      static_cast<std::size_t>(num_threads);

      for (std::size_t i = start; i < end; ++i) {
        tasks[i].result = tasks[i].cb(g, tasks[i].src, tasks[i].K);
      }
    });
  }

  for (std::thread& worker : workers) {
    worker.join();
  }
}

int main() {

  CSRGraph g{
      4,
      {0, 2, 3, 4, 4},
      {1, 2, 3, 3}
  };

  QueryCallback out_degree =
      [](const CSRGraph& graph, int src, int) {
        return std::to_string(
            graph.offsets[src + 1] - graph.offsets[src]
        );
      };

  std::vector<QueryTask> tasks;

  for (int src = 0; src < 4; ++src) {
    tasks.push_back({src, 0, out_degree, ""});
  }

  // Run the queries using 2 threads.
  RunTasksParallel(g, tasks, 2);

  // Print results.
  for (const QueryTask& task : tasks) {
    std::cout << "src " << task.src << ": out-degree = " << task.result << '\n';
  }

  return 0;
}