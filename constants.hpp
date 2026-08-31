#pragma once

#include <cstdint>
#include <utility>

const int BOARD_SIZE = 8;
const char EMPTY = '.';
const char VALID = '_';
const char PLAYER1 = 'B';
const char PLAYER2 = 'W';
const int DEFAULT_DEPTH = 5;
const int DEFAULT_TIME_LIMIT = 5;

// Board state: one bit per square (square = row*BOARD_SIZE + col, bit 0 =
// row 0/col 0), set in black_bb/white_bb according to which player occupies
// it. A square set in neither is empty; a square is never set in both.
extern uint64_t black_bb;
extern uint64_t white_bb;
extern std::pair<int,int> last_move;

// Weighted piece counter for the evaluation function
extern const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Weighted piece counter for endgame
extern const int ENDGAME_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Per-phase evaluation weights
struct PhaseWeights { int material, mobility, stability, frontier; };
extern const PhaseWeights PHASE_WEIGHTS[5];