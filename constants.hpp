#pragma once

#include <array>
#include <utility>

const int BOARD_SIZE = 8;
const char EMPTY = '.';
const char VALID = '_';
const char PLAYER1 = 'B';
const char PLAYER2 = 'W';
const int DEFAULT_DEPTH = 5;
const int DEFAULT_TIME_LIMIT = 5;

using Board = std::array<std::array<char, BOARD_SIZE>, BOARD_SIZE>;
extern Board board;
extern std::pair<int,int> last_move;

// Weighted piece counter for the evaluation function
extern const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Weighted piece counter for endgame
extern const int ENDGAME_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Per-phase evaluation weights
struct PhaseWeights { int material, mobility, stability, frontier; };
extern const PhaseWeights PHASE_WEIGHTS[5];