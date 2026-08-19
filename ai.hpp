#pragma once

#include <utility>
#include <vector>
#include <chrono>
#include <cstdint>
#include "constants.hpp"

// Total negascout() invocations during the most recent predict_move() call.
// Reset at the start of predict_move(); useful for headless benchmarking
// (see main.cpp's --selfplay/--search flags) and later for measuring TT hit rate.
extern uint64_t node_count;

int game_phase();
int evaluate_board(char player, int phase);
std::vector<std::pair<int, int>> get_sorted_moves(char player);
int negascout(int depth, int alpha, int beta, char player);
std::pair<int, int> predict_move(char player, int time_limit, int& out_score, int& out_depth);