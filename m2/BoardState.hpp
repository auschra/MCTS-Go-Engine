#pragma once 
#include <cstdint>
#include <stdio.h>
#include <cstdio>

using u128 = unsigned __int128;


//TODO:


// - Expand functionality to NxN board (13x13, 19x19), 

// -----------------------
// Compile time functions 
// -----------------------

// Create valid 9x9 play area mask on 11x11 board
// For 9x9 board, 11x11 = 121bits -> int 128bit int = 16 bytes per mask. 
constexpr u128 create_border_mask() {

    u128 mask = 0;                                          // Initital blank 0

    // Assign 1s to 9x9 playable area
    for (int row = 1; row<=9; row++) {
        for (int col = 1; col <= 9; col++) {
            int index = (row * 11) + col;                   // Get idx of inside 9x9

            mask = mask | ((u128)1 << index);               // Assign 1 to 9x9
        }
    }
    return mask;
}






// Historical move list of given start
// 81 x 8bit int of tile indexes + 1x 8bit move count = 83 bytes
struct MoveList {

    uint8_t moves[82];                                      // 9x9 + pass
    uint8_t move_count = 0;

    void add_move(uint8_t move_idx) {                       // Add given move to the array
        moves[move_count] = move_idx;
        move_count++;
    }
};









// ---------------------------
// Board class 
// =-------------------------
class BoardState {

private:

    // Create bitboards
    u128 black_stones = 0;
    u128 white_stones = 0;
    bool is_black_turn = true;
    uint8_t consecutive_passes = 0;

    // Create border mask
    static constexpr u128 border_mask = create_border_mask();      

    // Shift tiles in each direction using bitshift
   
    u128 shift_left(u128 stones) const {return (stones << 1) & border_mask;}
    u128 shift_right(u128 stones) const {return (stones >> 1) & border_mask;}
    u128 shift_up(u128 stones) const {return (stones << 11) & border_mask;}         // up / down whole row
    u128 shift_down(u128 stones) const {return (stones >> 11) & border_mask;}











    // Get adjacent tiles of all stones by shifting in all directions
    u128 get_adjacent_tiles(u128 stones) const {
        return shift_left(stones)  | 
               shift_right(stones) | 
               shift_up(stones)    |                    // bitwise or
               shift_down(stones);
    }








    // Check adjacent liberties
    bool check_liberties(u128 group, u128 occupied_tiles) const{
        
        u128 empty_tiles = (~occupied_tiles) & border_mask;                         // Find all free tiles within playable area 
        u128 touching = get_adjacent_tiles(group);                                  // Of these empty tiles, which are touching the group

        return (touching & empty_tiles) > 0;
    }










    // Capture logic 
    u128 get_stone_group(u128 start_stone, u128 player_stones) const {

        u128 group = start_stone;                                                       // create new group
        u128 leading_edge = start_stone;                                                // only recompute edge

        while (leading_edge > 0) {
            u128 adjacent_stones = get_adjacent_tiles(group) & player_stones;           // Find all the adjacent stones to group and filter by friendly
            leading_edge = adjacent_stones & ~group;                                     // New neighbour stones
            group |= leading_edge;                                                         // birwise add group to add these new stones (not recursive anymore)

        }
        return group;
    }
    









// -----------------------------
// Public functions
// -----------------------------
// - what are the legal moves
    // - check the free spaces available
    // - check whether piece was captured
    // - check whether suicide
// - execute a move
// - check whether the game is over
// - tally stones to calculate score

public:

    // Create new board
    BoardState() = default;

    // Get mask of empty tiles
    inline u128 get_empty_mask() const {
        return border_mask & ~(black_stones | white_stones);
    }






    // *NEW* public acces for rollout
    // count trailing zeroes to get idx of the first bit inarray
    static inline int get_tile_idx(u128 space_mask) {
        uint64_t first_half = (uint64_t)space_mask;                       // Get first 64 bis
        if (first_half != 0) return __builtin_ctzll(first_half);          // Found bit in first 64 bit (right side) of 128 bit
        return 64 + __builtin_ctzll((uint64_t)(space_mask >> 64));        // Must be in left side then
    }




    // *NEW*
    // - Fix to reduce checking legality of all moves for random select during rollout
    // - Now will be called on the candidate idx, sample new idx if not legal
    bool is_legal_move(int idx) const {

         // TODO:
            // - move checkign adjacent to vectorised operation
            // - dont allow friendly eye fills
            // - other rule to add & commment clean up

        u128 check_stone = ((u128)1 << idx);
        u128 my_stones = is_black_turn ? black_stones : white_stones;
        u128 opp_stones = is_black_turn ? white_stones : black_stones;
        u128 occ_stones = my_stones | opp_stones;
        u128 empty_mask = border_mask | ~occ_stones;

        u128 adjacent = get_adjacent_tiles(check_stone);

        // Rules to check
        // 0. If adjacent liberty -> always legal
        if ((adjacent & empty_mask) != 0) {
            return true;
        }

        // 0. Check for captures
        u128 adj_opps = adjacent & opp_stones;
        u128 cand_occ = occ_stones | check_stone;

        while (adj_opps > 0) {
            u128 single_opponent = adj_opps & (-adj_opps);
            u128 group_opponent = get_stone_group(single_opponent, opp_stones);
            if (!check_liberties(group_opponent, cand_occ)) return true;
            adj_opps &= ~group_opponent;
        }

        // Check suicide
        u128 potential_stones = my_stones | check_stone;
        u128 potential_group = get_stone_group(check_stone, potential_stones);
        
        if (check_liberties(potential_group, cand_occ)) return true;

        return false;
        }









    // Full list of legal moves on state s
    // Used only by expansion phase to create all possible children nodes. 
    MoveList get_legal_moves() const {
        MoveList list;

        u128 my_stones = is_black_turn ? black_stones : white_stones;              // 
        u128 opponent_stones = is_black_turn ? white_stones : black_stones;        // Assign others to opponent
        u128 occupied_stones = my_stones | opponent_stones;                        // Union of both bitboards = occupied

        u128 empty_mask = border_mask & ~occupied_stones;                           // playable and not occupied
        u128 tiles_to_check = empty_mask;                                            // Check each empty
        
        while (tiles_to_check > 0) {

            int check_idx = get_tile_idx(tiles_to_check);       // Get idx for first free space

            u128 check_stone = ((u128)1 << check_idx);          // Single stone bitboard of this space
            tiles_to_check &= (tiles_to_check -1);              // Remove this space from check_list (flip last bit and intersect over original int )

            
            u128 adjacent = get_adjacent_tiles(check_stone);    // If there is a liberty next to this stone, automatically legal
            if ((adjacent & empty_mask) != 0) {                 // Add this free space to move list
                list.add_move(check_idx);
                continue;
            }

            // No adjacent spaces, so must capture something to be legal
            u128 adjacent_opponents = adjacent & opponent_stones;         
            u128 potential_occupied = occupied_stones | check_stone;
            bool capture = false;

            // Adjacent stones around so must capture something to be legal?
            while (adjacent_opponents > 0) {
                        u128 single_opponent = adjacent_opponents & (-adjacent_opponents);
                        u128 group_opponent = get_stone_group(single_opponent, opponent_stones);

                        // If this placed stone means opponent has no liberties, get captured, legal move
                        if (!check_liberties(group_opponent, potential_occupied)) {
                            capture = true;
                            break;
                        }

                    adjacent_opponents &= ~group_opponent;                      
                    }

                    // Add the capture move, will create new liberty (by removing opponent stone)
                    if (capture) {
                        list.add_move(check_idx);
                        continue;
                    }

                    // Check suicide
                    u128 potential_stones = my_stones | check_stone;
                    u128 potential_group = get_stone_group(check_stone, potential_stones);

                    if (check_liberties(potential_group, potential_occupied)) {         // if joining to a group with a liberty, not suicide
                        list.add_move(check_idx);
        }
    }

    list.add_move(121);             // can always pass
    return list;
}

    int final_score() const {
        
        // Split board into high/low sides for intrinsic bit count
        uint64_t black_low = (uint64_t)black_stones;
        uint64_t black_high = (uint64_t)(black_stones >> 64);
        uint64_t white_low = (uint64_t)white_stones;
        uint64_t white_high = (uint64_t)(white_stones >> 64);

        int black_count = __builtin_popcountll(black_low) + __builtin_popcountll(black_high);   
        int white_count = __builtin_popcountll(white_low) + __builtin_popcountll(white_high);
        int black_score = black_count * 10;
        int white_score = (white_count * 10) + 75;          // Scale by 10 to avoid 7.5 float mult

        if (black_score > white_score) return 1;
        if (white_score > black_score) return -1;
        return 0;
    }
   
    // Add selected move to board
    void make_move(int index) {


        // Pass turn
        if (index == 121) {
            consecutive_passes++;
            is_black_turn = !is_black_turn;
            return;
        }

        consecutive_passes = 0;                     // no pass so reset counter

       
        u128 new_stone = ((u128) 1 << index);                                           // New stone mask
        
        // current and opposition board
        u128& player_stones = is_black_turn ? black_stones : white_stones;
        u128& opponent_stones = is_black_turn ? white_stones : black_stones;

        
        player_stones = player_stones | new_stone;                                              // place stone

       
        u128 adjacent_opponent = get_adjacent_tiles(new_stone) & opponent_stones;            // find touching enemy stones to placed stone

        while (adjacent_opponent > 0) {

            u128 single_opponent = adjacent_opponent & (-adjacent_opponent);

            //connect single opp to its group
            u128 opponent_group = get_stone_group(single_opponent, opponent_stones);
            u128 occupied = player_stones | opponent_stones;

            if (!check_liberties(opponent_group, occupied)) {
                // take!

                opponent_stones = opponent_stones & ~opponent_group;
            }

            adjacent_opponent = adjacent_opponent & ~opponent_group;

        }

        is_black_turn = !is_black_turn;
    }

    bool player_turn() const {
        return is_black_turn;
    }

    bool is_terminal() const {
        return consecutive_passes >= 2;
    }
};
