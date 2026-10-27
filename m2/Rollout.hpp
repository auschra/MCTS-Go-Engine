#pragma once
#include "BoardState.hpp"
#include <random>


// Rollout strategy
// Board state at leaf node -> ie no children will be passed to gpu -> multiple threads will perform random rollouts from this board state

// multiple board states will be passed to gpu from single tree  (openmp multiple threads running sims from root at once ->)

// Multiple trees will be running on different cores 

// Gpu will simply be calculating rewards -> passing this back to CPU to update value and visit counts ?



class Simulator {



// Random generator for action selection
private:
    uint64_t rng_state;         // persistent

    static inline uint64_t wyrand(uint64_t &seed) {

        seed += 0xa0761d6478bd642full;      // iterate seed, ensure cover 2^64 before repeat

        __uint128_t store_128bit = (__uint128_t)(seed ^ 0xe7037ed1a0b428dbull) * seed;             // Scramble seed * seed (64x64 bit )

        return (uint64_t)(store_128bit >> 64) ^ (uint64_t)store_128bit;                             // Bitwise upper and lower xor
    }





    // Map random 64bit int to random idx of range
    static inline uint32_t rand_idx(uint64_t wyrand_out, uint32_t idx_range) {                          // Fast mapping w/o modulo div
        
        uint32_t rand_32 = (uint32_t)wyrand_out;                                                        // Clip to 32 bit from action space ceiling (82)
        
        return (uint32_t)(((uint64_t)rand_32 * (uint64_t)idx_range) >> 32);                             // bit shift by 32 to divide rand_32 * Range  by 2^32
    }






    public:
    Simulator(uint64_t seed_val) : rng_state(seed_val) {}







    
    int rollout(BoardState& state) {

        int depth = 0;
        int max_depth = 200;                                    // max game length


        // Rollout until terminal state reached
        while (!state.is_terminal() && depth < max_depth) {

            u128 empty_mask = state.get_empty_mask();






            // Get empty tiles
            int empty_indices[82];
            uint32_t empty_count = 0;






            u128 temp_mask = empty_mask;
            while (temp_mask > 0) {
                empty_indices[empty_count++] = BoardState::get_tile_idx(temp_mask);
                temp_mask &= (temp_mask - 1); 
            }








            if (empty_count == 0) {
                state.make_move(121); // Pass
                depth++;
                continue;
            }




            bool move_made = false;
            // Pick random moves until we find a legal one
            while (empty_count > 0) {
                uint64_t raw_rand = wyrand(rng_state);
                uint32_t random_idx = rand_idx(raw_rand, empty_count);
                int candidate = empty_indices[random_idx];

                if (state.is_legal_move(candidate)) {
                    state.make_move(candidate);
                    move_made = true;
                    break;
                } else {
                    // Illegal move: swap with the last element and shrink range in O(1)
                    empty_indices[random_idx] = empty_indices[empty_count - 1];
                    empty_count--;
                }
            }

            if (!move_made) {
                state.make_move(121); // Pass if all empty spaces are illegal/eyes
            }
            
            depth++;
        }

        return state.final_score();
    }
};