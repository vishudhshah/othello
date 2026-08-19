#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <vector>
#include "constants.hpp"

std::string player_name(char player);

// Returns the occupant of (row, col): PLAYER1, PLAYER2, or EMPTY. The only
// square-level read accessor exposed for the bitboard representation —
// used by ui.cpp for rendering, since the board itself is two uint64_t
// masks (black_bb/white_bb, constants.hpp) rather than a per-cell array.
char cell_at(int row, int col);

void initialize_board();
bool parse_fen(const std::string& fen);
bool parse_64char(const std::string& s);
void make_move(int row, int col, char player);

// Undo record for make_move_undoable()/unmake_move(): the search hot path's
// alternative to a full-board copy+restore. With the bitboard representation
// this is just the squares that flipped — the placed disc plus flip_bb is
// enough to reverse the move exactly.
struct MoveUndo {
    int row, col;
    char player;
    uint64_t flip_bb;
};
MoveUndo make_move_undoable(int row, int col, char player);
void unmake_move(const MoveUndo& undo);

bool is_valid_move(int row, int col, char player);
bool is_game_over();
bool turn_skip(char player);
std::vector<std::pair<int, int>> compute_valid_moves(char player);
std::pair<int, int> calculate_scores();
void export_game(const std::vector<std::pair<char, std::string>>& moves, const std::vector<int>& ai_scores, const std::vector<int>& ai_depths, int game_mode, char player_color, int time_limit_b, int time_limit_w, const std::string& start_pos = "", char resigned_by = '\0');