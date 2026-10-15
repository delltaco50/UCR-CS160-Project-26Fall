// g++ -std=c++20 -pthread phase-2/week-3.cpp -o phase-2/a.out && ./phase-2/a.out

// Includes the entire std library for C++
#include <bits/stdc++.h>
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
    virtual bool HasWork() const = 0;

    // Process one vertex. tid ∈ [0, nthreads). The serial runner calls with
    // tid=0; the parallel runner calls with each thread's ID.
    virtual void Process(int tid, uint32_t v, const CsrGraph& g) = 0;

    // End of round. Merge per-thread buffers, prepare state for next round.
    virtual void PostRound() = 0;
};

// Serial BSP runner. Calls the algorithm's methods in a loop until HasWork() returns false.
void BspSerial(const CsrGraph& g, BspAlgorithm& algo) {
  while (algo.HasWork()) {
    for (uint32_t v = 0; v < g.num_vertices; v++) {
        algo.Process(/*tid=*/0, v, g);
    }
    algo.PostRound();
  }
}

// Parallel BSP runner. Calls the algorithm's methods in a loop until HasWork() returns false.
void BspParallel(const CsrGraph& g, BspAlgorithm& algo, int nthreads);

// Breadth-first search algorithm. Computes the distance from a source vertex to all other vertices.
class Bfs : public BspAlgorithm {
public:
// Constructs a BFS algorithm for a graph with num_vertices vertices, starting from source vertex.
    Bfs(uint32_t num_vertices, uint32_t source, int nthreads) {
        dist_.assign(num_vertices, -1);
        dist_prev_.assign(num_vertices, -1);

        dist_[source] = 0;
        dist_prev_[source] = 0;

        tl_updated_.assign(nthreads, 0);
        any_updated_ = true;
    }

    // Returns true if any vertex was updated in the last round.
    bool HasWork() const override { return any_updated_; }

    // Processes a vertex v. If v has not been visited, checks its in-neighbors to see if any have been visited. 
    //If so, updates v's distance and marks it as updated.
    void Process(int tid, uint32_t v, const CsrGraph& g) override {
        if (dist_prev_[v] != -1) { return; }

        for (uint32_t i = g.in_offsets[v];
            i < g.in_offsets[v + 1]; i++) {

            uint32_t u = g.in_edges[i];

            if (dist_prev_[u] != -1) {
                dist_[v] = dist_prev_[u] + 1;
                tl_updated_[tid] = 1;
                break;
            }
        }
    }

    // Merges per-thread buffers and prepares for the next round. If any vertex was updated, sets any_updated_ to true.
    void PostRound() override {
        any_updated_ = false;

        for (size_t i = 0; i < tl_updated_.size(); i++) {
            if (tl_updated_[i]) {
                any_updated_ = true;
            }
            tl_updated_[i] = 0;
        }

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

int main() {
    // Load the graph from the specified file.
    CsrGraph g = LoadGraph("phase-2/soc-LiveJournal1-weighted.txt");

    Bfs bfs(g.num_vertices, 0, 1);

    BspSerial(g, bfs);

    // Print the number of vertices, outgoing edges, incoming edges, and the BFS distance from vertex 0 to vertex 50.
    cout << "Vertices: " << g.num_vertices << endl;
    cout << "Outgoing edges: " << g.edges.size() << endl;
    cout << "Incoming edges: " << g.in_edges.size() << endl;
    cout << "BFS distance from 0 to 50: " << bfs.distance(50) << endl;

    return 0;
}
