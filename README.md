# MCTS-Go-Engine

src 
- Purely serial implementation
- Only simulates one round of play (board state not reset to new position)

m2
- leaf, root, tree parallelisation
- moved to SoA for AVX intrinsics
- several efficiency improvements
