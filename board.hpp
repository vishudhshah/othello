#pragma once

#include <utility>
#include "constants.hpp"

void initialize_board();
void print_board();
void print_highlighted_board(char player);
void make_move(int row, int col, char player);
bool is_valid_move(int row, int col, char player);
bool is_game_over();
bool turn_skip(char player);
std::pair<int, int> calculate_scores();
void print_scores();
void print_winning_message(); 