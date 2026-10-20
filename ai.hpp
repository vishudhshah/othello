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

// Shared soft deadline for time-boxed searches. predict_move() sets it before
// starting IDDFS; negascout() checks it every node and aborts (via the
// search_aborted flag) when exceeded, so one deep subtree can no longer run
// arbitrarily past the requested time limit. The fixed-depth --search
// benchmark path leaves it unset (steady_clock::time_point()), disabling the
// checks entirely so fixed-depth node counts stay exact and deterministic.
extern std::chrono::steady_clock::time_point search_deadline;
extern bool search_aborted;

int game_phase();
int evaluate_board(char player, int phase);
std::vector<std::pair<int, int>> get_sorted_moves(char player);
int negascout(int depth, int alpha, int beta, char player);
std::pair<int, int> predict_move(char player, int time_limit, int& out_score, int& out_depth);