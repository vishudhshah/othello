#pragma once

#include <vector>

const int BOARD_SIZE = 8;
const char EMPTY = '.';
const char VALID = '_';
const char PLAYER1 = 'B';
const char PLAYER2 = 'W';
const int DEFAULT_DEPTH = 5;
const int DEFAULT_TIME_LIMIT = 5;

extern std::vector<std::vector<char>> board;

// Weighted piece counter for the evaluation function
extern const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE];

// Weighted piece counter for endgame
extern const int ENDGAME_WEIGHTS[BOARD_SIZE][BOARD_SIZE];