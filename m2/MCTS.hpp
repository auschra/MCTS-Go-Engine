#pragma once
#include <cmath>
#include <vector>
#include <limits>
#include <utility>
#include <fstream>
#include "BoardState.hpp"
#include "Rollout.hpp"


// Paralleisation

// TODO: 

// Move to SoA to allow AVX intrinsics for UCB scores

/*
// Store node variables as parallel arrays to allow AVX
struc NodeSoA {
    i





}


*/



struct Node {

    int visits = 0;                     // number of times this node has been visited
    int move_idx = -1;               // previous move led us here max 91
    double value = 0;                   // node score for win / loss
    int parent = -1;                    // placeholders
    int first_child = -2;               //
    uint8_t num_children = 0;               // max 91 children



    // Vectorised UCV
    float ucb_score(double log_pvis_vec, float c = 1.414) const {


        // for (__int644 )

        if (visits == 0) {
            return std::numeric_limits<float>::infinity();                     // score = infinite if no visits
        }

        double exploit = value / visits;
        double explore = c * std::sqrt(log_pvis_vec / visits);
        return exploit + explore;
    }
};

class MCTS {
private:
    std::vector<Node> tree;             // create vector to hold nodes
    int node_count = 0;

public:









    // Create root node, expand tree vec to hold max node size
    MCTS(int max_nodes = 10000) {               // max_nodes determined by iter size
        tree.resize(max_nodes);
        tree[0] = Node();
        node_count = 1;
    }








    // traverse tree from root until new leaf node found (best score -> best score etc)
    int select_leaf_node(BoardState& current_state) {

        int current_node_idx = 0;

        while (tree[current_node_idx].num_children > 0) {                   // we have seen this node before if it has children

            int best_child_idx = -1;                                        // uninitialised
            double best_ucb = -std::numeric_limits<double>::infinity();     // unitialised
            int first = tree[current_node_idx].first_child;                 // Avoid calling these in loop 
            int num = tree[current_node_idx].num_children;                  //
            int parent_visits = tree[current_node_idx].visits;              //
            
            // precalc log(parent_visits) since doesnt change for children
            double log_parent = std::log(parent_visits > 0 ? parent_visits : 1.0);              

            for (int i = 0; i < num; i++) {
                int child_idx = first + i;                                  // iterate through children (stored adjacen)
                double ucb = tree[child_idx].ucb_score(log_parent);         // calc score

                if (ucb > best_ucb) {                                       // Find highest child score
                    best_ucb = ucb;
                    best_child_idx = child_idx;
                }
            }

            current_node_idx = best_child_idx;      
            current_state.make_move(tree[current_node_idx].move_idx);       // move to child with highest score and repeat until no children
        }

    return current_node_idx;
    
    }










    // At Node with no children, get all legal moves
    int expand_node(int leaf_idx, BoardState& current_state) {

        // End of game
        if (current_state.is_terminal()) {
            return leaf_idx;
        }

        // FIXME:

        // If new state 
        if (current_state.visits ==0) {
            return leaf_idx;
        }

        MoveList moves = current_state.get_legal_moves();                   // All possible moves from this position
 
        if (node_count + moves.move_count > tree.size()) {                  // IF we will hit the max tree size limit after expansion, stop to prevent memory reallocaiton
            return leaf_idx; 
        }
        

        // First block of children
        int first_child_idx = node_count;                           // idx of of next child (contiguous)
        tree[leaf_idx].first_child = first_child_idx;
        tree[leaf_idx].num_children = moves.move_count;             // 

        for (int i = 0; i < moves.move_count; i++) {

            int child_idx = node_count++;                   // New index (/ node ) for each possible move
            
            tree[child_idx] = Node();                       // Create a new node 
            tree[child_idx].parent = leaf_idx;              // Assign it current leaf as its parent
            tree[child_idx].move_idx = moves.moves[i];      // Assign it its move
        }

        current_state.make_move(tree[first_child_idx].move_idx);            // Pick the first child (all children have inf score)
        return first_child_idx;
    }




    // ****************** RACE CONDITION *****************

    // # pragma omp critical 

    // Single thread will be updating values, visit counts of node at a time

    // Different threads will update differnet parts of the tree? 


    // Mutex locks a. particular memory address from being written by other thread? 
    
    


    // Barrier to synchronise update.s. 




    // pass values up tree, flip on each step (white vs black)
    void backpropagate(int node_idx, int game_result, bool black_turn_leaf) {
        bool current_black_turn = black_turn_leaf;
        int current_node = node_idx;

        while (current_node != -1) {        // Stops at root node 
            tree[current_node].visits++;

            double reward = 0.5;
            if (game_result == 1) { 
                reward = current_black_turn ? 0.0 : 1.0;        // Black won
            } else if (game_result == -1) { 
                reward = current_black_turn ? 1.0 : 0.0;        // White won
            }

            tree[current_node].value += reward;                    // add reward
            current_node = tree[current_node].parent;              // move up
            current_black_turn = !current_black_turn;              // flip turn                            
        }
    }                                     









    // Execute 4 stage search process (iter times)
    int search(const BoardState& root_state, int iterations, uint64_t seed_val) {

        Simulator sim(seed_val);

        for (int i = 0; i < iterations; i++) {

            BoardState state = root_state;                                  // Start at blank board

            int leaf_idx = select_leaf_node(state);                         // 1. Select leaf node

            int sim_node_idx = expand_node(leaf_idx, state);                // 2. Expanded node idx

            int result = sim.rollout(state);                                // 3. Simulate game

            backpropagate(sim_node_idx, result, state.player_turn());       // 4. Backprop
            
            if (node_count >= tree.size() - 82) {                           // memory limit
                break;
            }
        }

        // Blank start 
        int best_move = -1;
        int max_visits = -1;
        int first = tree[0].first_child;                                
        int num = tree[0].num_children;


        // Pick move with most visits (best subtree)
        for (int i = 0; i < num; i++) {     
            int child_idx = first + i;

            if (tree[child_idx].visits > max_visits) {
                max_visits = tree[child_idx].visits;
                best_move = tree[child_idx].move_idx;
            }
        }

        return best_move;
    }

    // Reset to single root node
    void reset() {  
        tree[0] = Node();
        node_count = 1; 
    }


};