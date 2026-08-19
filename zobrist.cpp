#include "zobrist.hpp"
#include <random>

uint64_t ZOBRIST_TABLE[2][BOARD_SIZE * BOARD_SIZE];
uint64_t ZOBRIST_TURN;
uint64_t current_hash = 0;

void init_zobrist_table() {
    // Fixed seed (not random_device): deterministic hashes make incremental-
    // update bugs reproducible across runs instead of only sometimes firing.
    std::mt19937_64 rng(0xC0FFEE1234567890ULL);
    for (int p = 0; p < 2; p++)
        for (int sq = 0; sq < BOARD_SIZE * BOARD_SIZE; sq++)
            ZOBRIST_TABLE[p][sq] = rng();
    ZOBRIST_TURN = rng();
}

uint64_t compute_hash(uint64_t black_bb, uint64_t white_bb) {
    uint64_t h = 0;
    uint64_t bb = black_bb;
    while (bb) { int sq = __builtin_ctzll(bb); h ^= ZOBRIST_TABLE[0][sq]; bb &= bb - 1; }
    bb = white_bb;
    while (bb) { int sq = __builtin_ctzll(bb); h ^= ZOBRIST_TABLE[1][sq]; bb &= bb - 1; }
    return h;
}
