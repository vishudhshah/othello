#pragma once

#include <utility>
#include <vector>
#include <chrono>
#include "constants.hpp"

int game_phase();
int evaluate_board(char player);
std::vector<std::pair<int, int>> get_sorted_moves(char player);
int negamax(int depth, int alpha, int beta, char player);
std::pair<int, int> predict_move(char player, int time_limit);