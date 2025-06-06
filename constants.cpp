#include "constants.hpp"

std::vector<std::vector<char>> board(BOARD_SIZE, std::vector<char>(BOARD_SIZE, EMPTY));

// Weighted piece counter for the evaluation function
const int POSITION_WEIGHTS[BOARD_SIZE][BOARD_SIZE] = {
    {200,  -10, 20, 20, 20, 20,  -10, 200},
    {-10, -100,  1,  1,  1,  1, -100, -10},
    { 20,    1,  5,  5,  5,  5,    1,  20},
    { 20,    1,  5,  5,  5,  5,    1,  20},
    { 20,    1,  5,  5,  5,  5,    1,  20},
    { 20,    1,  5,  5,  5,  5,    1,  20},
    {-10, -100,  1,  1,  1,  1, -100, -10},
    {200,  -10, 20, 20, 20, 20,  -10, 200}
};

// Weighted piece counter for endgame
const int ENDGAME_WEIGHTS[BOARD_SIZE][BOARD_SIZE] = {
    {200,   5, 10, 10, 10, 10,   5, 200},
    {  5,  -5,  1,  1,  1,  1,  -5,   5},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    { 10,   1,  1,  1,  1,  1,   1,  10},
    {  5,  -5,  1,  1,  1,  1,  -5,   5},
    {200,   5, 10, 10, 10, 10,   5, 200}
}; 