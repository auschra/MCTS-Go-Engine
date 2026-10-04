#pragma once
#include "BoardState.hpp"
#include <random>

class Simulator {
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
        int max_depth = 300;                                    // max game length

        while (!state.is_terminal() && depth < max_depth) {

            MoveList legal_moves = state.get_legal_moves();

            if (legal_moves.move_count == 0) {
                state.make_move(121);                                                       // Pass if no moves 
            } 
            else {
                uint64_t raw_rand = wyrand(rng_state);
                uint32_t random_idx = rand_idx(raw_rand, legal_moves.move_count);           // Pick a random legal move
                
                state.make_move(legal_moves.moves[random_idx]);                             // Apply move
            }
            depth++;
        }

        return state.final_score();
    }
};