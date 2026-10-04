
#include <iostream>
#include <chrono>
#include <cstdlib>
#include "BoardState.hpp"
#include "Rollout.hpp"
#include "MCTS.hpp"

int main(int argc, char* argv[]) {

    int iterations = 100000;                
    uint64_t seed = 42;

    // iter sweep
    if (argc > 1) {iterations = std::atoi(argv[1]);}
    if (argc > 2) {seed = std::strtoull(argv[2], nullptr, 10);}

    BoardState root_board;

    int max_required_nodes = 1 + (iterations * 82);     // max number of nodes (n_iterations for each move + root node) 
    MCTS mcts_engine(max_required_nodes);       // preallocate

    auto start_time = std::chrono::high_resolution_clock::now();
    int best_move = mcts_engine.search(root_board, iterations, seed); 
    auto end_time = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end_time - start_time;

    // Output: : iters, time in sec, choose move 
    std::cout << iterations << "," << elapsed.count() << "," << best_move << std::endl;

    return 0;
}