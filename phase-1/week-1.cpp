// g++ -std=c++20 -pthread phase-1/week-1.cpp -o phase-1/week-1 && ./phase-1/week-1
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


struct CSRGraph {
    int num_vertices;
    std::vector<int> offsets;  // size: num_vertices + 1
    std::vector<int> edges;    // destination IDs, grouped by source vertex
};

CSRGraph LoadGraph(const char *filename){
    CSRGraph graph;
    std::ifstream input(filename);
    std::string line;

    int current_src = 0;
    int max_vertex = -1;
   
    graph.offsets.push_back(0);

    while (std::getline(input, line)) {
        // Skip comments, then parse src and dst from this line.
        if (line[0] == '#'){
            continue;
        }
        // initialized istringsteam with line string
        std::istringstream vertex(line);
        int src, dst;

        if (vertex >> src >> dst){
            // Add offsets for any new source vertices
            while (current_src < src) {
                graph.offsets.push_back(graph.edges.size());
                current_src++;
            }

            // Store destination directly
            graph.edges.push_back(dst);

            if (src > max_vertex) {
                max_vertex = src;
            }

            if (dst > max_vertex) {
                max_vertex = dst;
            }

        } else {
            std::cout << "Error: Failed to read two valid integers from the line.\n";
        }

       
    }
    graph.num_vertices= max_vertex + 1;

    // Add offsets for remaining vertices
    while (graph.offsets.size() < graph.num_vertices + 1) {
        graph.offsets.push_back(graph.edges.size());
    }

    return graph;
};

int main(){
    CSRGraph g = LoadGraph("../soc-Slashdot0902.txt");
    int v = 6;
    int out_degree = g.offsets[v + 1] - g.offsets[v];
    for (int i = g.offsets[v]; i < g.offsets[v + 1]; ++i) {
        std::cout << g.edges[i] << ' ';
    }

    std::cout << "num_vertices = " << g.num_vertices << '\n';
    std::cout << "edges.size() = " << g.edges.size() << '\n';
    std::cout << "vertex 6 out-degree = " << out_degree << '\n';
    return 0;
}