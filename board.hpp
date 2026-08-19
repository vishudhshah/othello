#pragma once

#include <utility>
#include <string>
#include <vector>
#include "constants.hpp"

std::string player_name(char player);
void initialize_board();
bool parse_fen(const std::string& fen);
bool parse_64char(const std::string& s);
void make_move(int row, int col, char player);

// Undo record for make_move_undoable()/unmake_move(): the search hot path's
// alternative to a full Board copy+restore. 19 is the maximum number of
// discs a single Othello move can flip.
struct MoveUndo {
    int row, col;
    char player;
    int flip_count;
    std::pair<int, int> flipped[19];
};
MoveUndo make_move_undoable(int row, int col, char player);
void unmake_move(const MoveUndo& undo);

bool is_valid_move(int row, int col, char player);
bool is_game_over();
bool turn_skip(char player);
std::vector<std::pair<int, int>> compute_valid_moves(char player);
std::pair<int, int> calculate_scores();
void export_game(const std::vector<std::pair<char, std::string>>& moves, const std::vector<int>& ai_scores, const std::vector<int>& ai_depths, int game_mode, char player_color, int time_limit_b, int time_limit_w, const std::string& start_pos = "", char resigned_by = '\0');