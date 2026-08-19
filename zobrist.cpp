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

uint64_t compute_hash(const Board& b) {
    uint64_t h = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (b[i][j] == PLAYER1) h ^= ZOBRIST_TABLE[0][i * BOARD_SIZE + j];
            else if (b[i][j] == PLAYER2) h ^= ZOBRIST_TABLE[1][i * BOARD_SIZE + j];
        }
    }
    return h;
}
