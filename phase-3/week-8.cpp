// Build: g++ -O2 -std=c++17 -pthread -fopenmp phase-3/week-8.cpp -o phase-3/week-8.out && ./phase-3/week-8.out

// Includes the entire std library for C++
#include <bits/stdc++.h>
#include <omp.h>
using namespace std;


struct CsrGraph {
    uint32_t num_vertices;
    std::vector<uint32_t> offsets;      // out-edges, size = num_vertices + 1
    std::vector<uint32_t> edges;        // out-neighbor IDs
    std::vector<int> weights;           // edge weights (parallel to edges)
    std::vector<uint32_t> in_offsets;   // in-edges (reverse CSR)
    std::vector<uint32_t> in_edges;     // in-neighbor IDs
    std::vector<int> in_weights;        // reverse edge weights (parallel to in_edges)
};


// Abstract base class for BSP algorithms. The algorithm is responsible for
// maintaining its own state, including per-thread buffers. The BSP runner
// calls the algorithm's methods in a loop until HasWork() returns false.
class BspAlgorithm {
public:
    virtual ~BspAlgorithm() = default;

    // Returns true if another round should be run.
    // Same for all 3 methods
    virtual bool HasWork() const = 0;

    // Process one vertex. tid ∈ [0, nthreads). The serial runner calls with
    // tid=0; the parallel runner calls with each thread's ID.
    virtual void Process(int tid, uint32_t v, const CsrGraph& g) = 0;

    // End of round. Merge per-thread buffers, prepare state for next round.
    // same logic for all 3 methods
    virtual void PostRound() = 0;
};

// Serial BSP runner. Calls the algorithm's methods in a loop until HasWork() returns false.
void BspSerial(const CsrGraph& g, BspAlgorithm& algo) {
  while (algo.HasWork()) {
    //go through every vertex in graph 
    for (uint32_t v = 0; v < g.num_vertices; v++) {
        //run algorithm with tid=0 because we use 1 thread
        algo.Process(/*tid=*/0, v, g);
    }
    algo.PostRound();
  }
}

// Parallel BSP runner. Calls the algorithm's methods in a loop until HasWork() returns false.
void BspParallel(const CsrGraph& g, BspAlgorithm& algo, int nthreads){

    // store number of vertices
    uint32_t num_vertices=g.num_vertices;
    vector<thread> workers;
    //reserve space for threads
    workers.reserve(nthreads);

    while (algo.HasWork()) {
    // Dynamically calculate chunk sizes per thread
        uint32_t chunk_size = (num_vertices + nthreads - 1) / nthreads;

        //create nthreads worker threads
        for (int tid = 0; tid < nthreads; tid++) {
            // calculate the first and vertex assigned to this thread
            uint32_t start_v = tid * chunk_size;
            // calculate one past the last assigned vertex.
            uint32_t end_v = std::min(start_v + chunk_size, num_vertices);

            // Launch thread to handle its block of vertices
            workers.emplace_back([&g, &algo, tid, start_v, end_v]() {
                // process only this threads vertices
                for (uint32_t v = start_v; v < end_v; v++) { algo.Process(tid, v, g); }
            });
        }

        // Global Barrier Synchronization: Wait for all threads to finish the current round
        for (auto& w : workers) { if (w.joinable()) { w.join();} }
        workers.clear();

    
        algo.PostRound();
    }

}

// Connected components algorithm. Computes the connected component ID for each vertex.
class CC : public BspAlgorithm{
public:
    // Initialize CC with one component label per vertex.
    // Each vertex initially uses its own ID as its label.    
    CC(uint32_t num_vertices, int nthreads) {
        dist_.resize(num_vertices);
        dist_prev_.resize(num_vertices);

        // Initialize each vertex's label to its own ID.
        for (uint32_t v = 0; v < num_vertices; v++) {
            dist_[v] = v;
            dist_prev_[v] = v;
        }

        tl_updated_.assign(nthreads, 0);
        any_updated_ = true;
    }

    bool HasWork() const override {return any_updated_;}

    // Processes vertex v by checking incoming and outgoing neighbors to see if a 
    // smaller component label can be found.
    void Process(int tid, uint32_t v, const CsrGraph& g) override {
        // Get the current best distance for vertex v.
        int best = dist_prev_[v];
        bool updated = false;

        // Loop through all incoming neighbors of vertex v.
        for (uint32_t i = g.in_offsets[v]; i < g.in_offsets[v + 1]; i++) {
            // Get the vertex ID of an incoming neighbor.
            uint32_t u = g.in_edges[i];

            // value dervived from the distance of the incoming neighbor.
            int candidate = dist_prev_[u];

            // If the candidate distance is less than the current best distance, update best and mark as updated.
            if (candidate < best) {
                best = candidate;
                updated = true;
            }
        }

        // Loop through all outgoing neighbors of vertex v.
        for (uint32_t i = g.offsets[v];i < g.offsets[v + 1]; i++) {
            // Get an outgoing neighbor of v.
            uint32_t u = g.edges[i];

            // Get the neighbor's component label.
            int candidate = dist_prev_[u];

            // Keep the smallest component label found.
            if (candidate < best) {
                best = candidate;
                updated = true;
            }
        }

        // If an update was made, set the distance for vertex v and 
        //mark the thread as updated.
        if (updated) {
            dist_[v] = best;
            tl_updated_[tid] = 1;
        }
    }

    // Merges per-thread buffers and prepares for the next round.
    void PostRound() override {
        any_updated_ = false;
        
        // go through update flag for each round
        for (size_t i = 0; i < tl_updated_.size(); i++) {

            // if any flag updated set any_updated_ to true so it can run another round
            if (tl_updated_[i]) {
                any_updated_ = true;
            }
            // reset flag for next update
            tl_updated_[i] = 0;
        }

        // copy current state into previous state  
        dist_prev_ = dist_;
    }

    int distance(uint32_t v) const { return dist_[v]; }

    private:
    vector<int> dist_;
    vector<int> dist_prev_;
    vector<uint8_t> tl_updated_;
    bool any_updated_ = true;
};

// Single source shortest path algorithm
class SSSP : public BspAlgorithm{
public:
//Constructs SSSP algorithm for a graph with num_vertices vertices, starting with source vertex to other verticies.
    SSSP(uint32_t num_vertices, uint32_t source, int nthreads) {
        dist_.assign(num_vertices, INT_MAX);
        dist_prev_.assign(num_vertices, INT_MAX);

        dist_[source] = 0;
        dist_prev_[source] = 0;

        tl_updated_.assign(nthreads, 0);
        any_updated_ = true;
    }

    // Returns true if any vertex was updated in the last round.
    bool HasWork() const override {return any_updated_;}

    // Processes vertex v by checking incoming neighbors
    // to see if a shorter weighted distance can be found.
    void Process(int tid, uint32_t v, const CsrGraph& g) override {
        // Get the current best distance for vertex v.
        int best = dist_prev_[v];
        bool updated = false;

        // Loop through all incoming neighbors of vertex v.
        for (uint32_t i = g.in_offsets[v]; i < g.in_offsets[v + 1]; i++) {
            // Get the vertex ID of an incoming neighbor.
            uint32_t u = g.in_edges[i];
            // If the incoming neighbor has not been discovered, skip it.
            if (dist_prev_[u] == INT_MAX) continue;
            // value from the distance of the incoming neighbor plus edge weight.
            int candidate = dist_prev_[u] + g.in_weights[i];
            // If the candidate distance is less than the current best distance, update best and mark as updated.
            if (candidate < best) {
                best = candidate;
                updated = true;
            }
        }
        // If an update was made, set the distance for vertex v and 
        //mark the thread as updated.
        if (updated) {
            dist_[v] = best;
            tl_updated_[tid] = 1;
        }
    }
    

    // Merges per-thread buffers and prepares for the next round.
    void PostRound() override {
        any_updated_ = false;
        // go through update flag for each round
        for (size_t i = 0; i < tl_updated_.size(); i++) {
            // if any flag updated set any_updated_ to true so it can run another round
            if (tl_updated_[i]) {any_updated_ = true;}
            // reset flag for next update
            tl_updated_[i] = 0;
        }
        // copy current state into previous state  
        dist_prev_ = dist_;
    }

    // Returns the distance from the source vertex to vertex v. If v is unreachable, returns -1.
    int distance(uint32_t v) const {return dist_[v];}

    private:
    vector<int> dist_;
    vector<int> dist_prev_;
    vector<uint8_t> tl_updated_;
    bool any_updated_ = true;
};

// Breadth-first search algorithm. Computes the distance from a source vertex to all other vertices.
class BFS : public BspAlgorithm {
public:
// Constructs a BFS algorithm for a graph with num_vertices vertices, starting from source vertex.
    BFS(uint32_t num_vertices, uint32_t source, int nthreads) {
        dist_.assign(num_vertices, -1);
        dist_prev_.assign(num_vertices, -1);

        dist_[source] = 0;
        dist_prev_[source] = 0;

        tl_updated_.assign(nthreads, 0);
        any_updated_ = true;
    }

    // Returns true if any vertex was updated in the last round.
    bool HasWork() const override {return any_updated_;}

    // Processes a vertex v. If v has not been visited, checks its in-neighbors to see if any have been visited. 
    //If so, updates v's distance and marks it as updated.
    void Process(int tid, uint32_t v, const CsrGraph& g) override {
        // Get the current best distance for vertex v.
        int best = dist_prev_[v];
        bool updated = false;
        if (best != -1) return;

        // Loop through all incoming neighbors of vertex v.
        for (uint32_t i = g.in_offsets[v]; i < g.in_offsets[v + 1]; i++) {
            // Get the vertex ID of an incoming neighbor.
            uint32_t u = g.in_edges[i];
            // If the incoming neighbor has not been discovered, skip it.
            if (dist_prev_[u] == -1) continue;
            // value dervived from the distance of the incoming neighbor plus 1.
            int candidate = dist_prev_[u] + 1;
            // If the candidate distance is less than the current best distance, update best and mark as updated.
            if (best == -1||candidate < best) {
                best = candidate;
                updated = true;
                break; // No need to check other neighbors if we found one that has been visited.
            }
        }
        // If an update was made, set the distance for vertex v and 
        //mark the thread as updated.
        if (updated) {
            dist_[v] = best;
            tl_updated_[tid] = 1;
        }
    }

    // Merges per-thread buffers and prepares for the next round.
    void PostRound() override {
        any_updated_ = false;
        // go through update flag for each round
        for (size_t i = 0; i < tl_updated_.size(); i++) {
            // if any flag updated set any_updated_ to true so it can run another round
            if (tl_updated_[i]) { any_updated_ = true; }
            // reset flag for next update
            tl_updated_[i] = 0;
        }
        // copy current state into previous state  
        dist_prev_ = dist_;
    }

    // Returns the distance from the source vertex to vertex v. If v is unreachable, returns -1.
    int distance(uint32_t v) const { return dist_[v]; }

private:
    vector<int> dist_;
    vector<int> dist_prev_;
    vector<uint8_t> tl_updated_;
    bool any_updated_ = true;
};

// Loads a graph from a file in the format: src dst weight. Ignores lines starting with #.
CsrGraph LoadGraph(const char* filename) {
    CsrGraph graph{};
    ifstream input(filename);

    // Check if the file was opened successfully. If not, throw a runtime error with a message.
    if (!input) {
        throw runtime_error(
            string("Cannot open graph: ") + filename
        );
    }

    string line;
    uint32_t src, dst;
    int weight;
    uint32_t max_vertex = 0;
    bool has_edges = false;

    // First pass: determine graph size.
    while (getline(input, line)) {
        istringstream ss(line);

        // Skip whitespace and comments.
        ss >> ws;
        if (ss.eof() || ss.peek() == '#') continue;

        // Read src, dst, and weight. If successful, update max_vertex and has_edges.
        if (ss >> src >> dst >> weight) {
            max_vertex = max(max_vertex, max(src, dst));
            has_edges = true;
        }
    }

    // Set the number of vertices. If there are edges, set num_vertices to max_vertex + 1; otherwise, set it to 0.
    graph.num_vertices = has_edges ? max_vertex + 1 : 0;

    graph.offsets.assign(graph.num_vertices + 1, 0);
    graph.in_offsets.assign(graph.num_vertices + 1, 0);

    // Second pass: count edges.
    input.clear();
    input.seekg(0);

    // Read each line of the input file. If the line is not empty or a comment, read src, dst, and weight. 
    // Increment the out-edge count for src and the in-edge count for dst.
    while (getline(input, line)) {
        istringstream ss(line);
        ss >> ws;
        if (ss.eof() || ss.peek() == '#') continue;

        if (ss >> src >> dst >> weight) {
            graph.offsets[src + 1]++;
            graph.in_offsets[dst + 1]++;
        }
    }

    // Compute prefix sums of offsets and in_offsets to get the starting index of each vertex's edges
    // in the edges and in_edges arrays.
    for (uint32_t i = 1; i <= graph.num_vertices; i++) {
        graph.offsets[i] += graph.offsets[i - 1];
        graph.in_offsets[i] += graph.in_offsets[i - 1];
    }

    uint32_t edge_count = graph.offsets.back();

    // Resize the edges, weights, in_edges, and in_weights vectors to hold all edges.
    graph.edges.resize(edge_count);
    graph.weights.resize(edge_count);
    graph.in_edges.resize(edge_count);
    graph.in_weights.resize(edge_count);

    vector<uint32_t> out_pos = graph.offsets;
    vector<uint32_t> in_pos = graph.in_offsets;

    // Third pass: fill forward and reverse CSR.
    input.clear();
    input.seekg(0);

    // Read each line of the input file. If the line is not empty or a comment, read src, dst, and weight.
    while (getline(input, line)) {
        istringstream ss(line);
        ss >> ws;
        if (ss.eof() || ss.peek() == '#') continue;

        // Read src, dst, and weight. If successful, add the edge to the graph's edges and weights arrays,
        // and add the reverse edge to the in_edges and in_weights arrays.
        if (ss >> src >> dst >> weight) {
            uint32_t p = out_pos[src]++;
            graph.edges[p] = dst;
            graph.weights[p] = weight;

            uint32_t q = in_pos[dst]++;
            graph.in_edges[q] = src;
            graph.in_weights[q] = weight;
        }
    }
    return graph;
}

// OpenMP BSP runner: static scheduling.
// The implicit barrier at the end of the omp parallel for completes the round
// before PostRound() updates the BFS state.
void BspParallelOmp(const CsrGraph& g, BspAlgorithm& algo, int nthreads) {
    while (algo.HasWork()) {
        #pragma omp parallel for num_threads(nthreads) schedule(static)
        for (int64_t v = 0; v < static_cast<int64_t>(g.num_vertices); ++v) {
            int tid = omp_get_thread_num();
            algo.Process(tid, static_cast<uint32_t>(v), g);
        }
        algo.PostRound();
    }
}

// OpenMP BSP runner: dynamic scheduling with a positive chunk size.
void BspParallelOmpDynamic(const CsrGraph& g, BspAlgorithm& algo,int nthreads, int chunk) {
    if (chunk <= 0) throw invalid_argument("OpenMP chunk must be positive");
    while (algo.HasWork()) {
        #pragma omp parallel for num_threads(nthreads) schedule(dynamic, chunk)
        for (int64_t v = 0; v < static_cast<int64_t>(g.num_vertices); ++v) {
            int tid = omp_get_thread_num();
            algo.Process(tid, static_cast<uint32_t>(v), g);
        }
        algo.PostRound();
    }
}

// Phase 2 workflow: write each run's answer file after timing is complete.
void WriteBfs(const BFS& bfs, uint32_t num_vertices, const string& filename) {
    ofstream output(filename);
    if (!output) throw runtime_error("Cannot write output: " + filename);
    for (uint32_t v = 0; v < num_vertices; ++v) {
        output << v << ' ' << bfs.distance(v) << '\n';
    }
}

int main(int argc, char** argv) {
    // Run from the project root by default, or provide explicit paths.
    const string graph_file = argc > 1 ? argv[1] : "phase-2/soc-LiveJournal1-weighted.txt";
    constexpr int nthreads = 4;
    constexpr int dynamic_chunk = 1024;

    try {
        CsrGraph g = LoadGraph(graph_file.c_str());
        if (g.num_vertices == 0) throw runtime_error("Graph is empty");

        omp_set_dynamic(0);  // Keep requested OpenMP team size at 4.
        cout << "Vertices: " << g.num_vertices << '\n';
        cout << "Edges: " << g.edges.size() << '\n';
        cout << "Source: 0, threads: " << nthreads << '\n';
        cout << fixed << setprecision(6);

        {
            BFS bfs(g.num_vertices, 0, nthreads);  // Fresh state.
            auto start = chrono::steady_clock::now();
            BspParallel(g, bfs, nthreads);          // Original Phase 2a runner.
            double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
            cout << "BFS (std::thread):    time = " << elapsed << '\n';
            WriteBfs(bfs, g.num_vertices, "BFS_std_thread.txt");
        }
        {
            BFS bfs(g.num_vertices, 0, nthreads);
            auto start = chrono::steady_clock::now();
            BspParallelOmp(g, bfs, nthreads);
            double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
            cout << "BFS (OpenMP static):  time = " << elapsed << '\n';
            WriteBfs(bfs, g.num_vertices, "BFS_omp_static.txt");
        }
        {
            BFS bfs(g.num_vertices, 0, nthreads);
            auto start = chrono::steady_clock::now();
            BspParallelOmpDynamic(g, bfs, nthreads, dynamic_chunk);
            double elapsed = chrono::duration<double>(chrono::steady_clock::now() - start).count();
            cout << "BFS (OpenMP dynamic): time = " << elapsed
                 << ", chunk = " << dynamic_chunk << '\n';
            WriteBfs(bfs, g.num_vertices, "BFS_omp_dynamic.txt");
        }
        return 0;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
