#pragma once

#include <utility>
#include <vector>
#include <chrono>
#include <cstdint>
#include "constants.hpp"

// Total negascout() invocations during the most recent predict_move() call.
// thread_local (Lazy SMP): each worker counts its own nodes; predict_move
// sums the workers' counts when the search finishes. Reset at the start of
// every search; useful for headless benchmarking (see main.cpp's
// --selfplay/--search flags) and for measuring TT hit rate.
extern thread_local uint64_t node_count;

// Shared soft deadline for time-boxed searches. predict_move() sets it before
// starting IDDFS; negascout() checks it every node and aborts (via the
// search_aborted flag) when exceeded, so one deep subtree can no longer run
// arbitrarily past the requested time limit. The fixed-depth --search
// benchmark path leaves it unset (steady_clock::time_point()), disabling the
// checks entirely so fixed-depth node counts stay exact and deterministic.
//
// Both are thread_local for Lazy SMP: the deadline is identical in every
// worker (written before spawn, read-only during the search) while the abort
// flag unwinds each worker's stack independently.
extern thread_local std::chrono::steady_clock::time_point search_deadline;
extern thread_local bool search_aborted;

// --- Lazy SMP ---
// Number of search threads for the next predict_move() (default 1 = the
// historical single-threaded search on the calling thread; set from
// main.cpp's --threads flag).
extern int search_threads;

// Upper bound on legal moves for one player: each legal move occupies a
// distinct empty square, so the count can never exceed 64 (one per board
// square). Positions with >24 legal moves genuinely occur at shallow search
// nodes with many empties — a 24 cap caused a stack-buffer-overflow caught
// by AddressSanitizer (fixed-depth 12, midgame FEN). 64 is the only safe
// static bound; the array costs 512B of stack per search frame.
constexpr int MAX_MOVES = 64;

int game_phase();
int evaluate_board(char player, int phase);

// Fills `out` (capacity >= MAX_MOVES) with the player's legal moves in
// square-priority order and returns the count. Zero allocation per call:
// runs once per search node.
int get_sorted_moves(char player, std::pair<int, int>* out);
int negascout(int depth, int alpha, int beta, char player);
std::pair<int, int> predict_move(char player, int time_limit, int& out_score, int& out_depth);