#pragma once

#include <cstdint>
#include "constants.hpp"

// Zobrist hashing: a 64-bit fingerprint of the board's occupied squares,
// maintained incrementally alongside every make_move (see board.cpp) so it
// never needs a full board scan to stay correct.
//
// current_hash intentionally excludes side-to-move: it is a pure function of
// which squares are occupied by which player. Callers that need a
// position+side-to-move key (transposition table, opening book) combine it
// with ZOBRIST_TURN themselves, e.g.:
//     uint64_t key = current_hash ^ (player == PLAYER2 ? ZOBRIST_TURN : 0);
// This keeps make_move/initialize_board/parse_fen/parse_64char from ever
// needing to know whose turn it is, and makes passes a non-issue for hash
// maintenance (no squares change on a pass, so current_hash is untouched;
// the caller's own `player` argument already reflects the turn change).

extern uint64_t ZOBRIST_TABLE[2][BOARD_SIZE * BOARD_SIZE]; // [0]=PLAYER1, [1]=PLAYER2, indexed by row*BOARD_SIZE+col
extern uint64_t ZOBRIST_TURN;
extern uint64_t current_hash;

// Seeds ZOBRIST_TABLE/ZOBRIST_TURN with a fixed seed (deterministic across
// runs, so hashes are reproducible for debugging/testing). Call once at
// program start, before any board setup.
void init_zobrist_table();

// Full recompute from scratch (occupancy only, no turn bit) — used to
// (re)initialize current_hash after a position is set up wholesale
// (initialize_board/parse_fen/parse_64char), and as the ground truth for
// verifying incremental updates never drift (see main.cpp's --verify-zobrist).
uint64_t compute_hash(const Board& b);
