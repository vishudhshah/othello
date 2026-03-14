#pragma once

#include <utility>
#include <string>
#include <vector>
#include "constants.hpp"

std::string player_name(char player);
void initialize_board();
bool parse_fen(const std::string& fen);
bool parse_64char(const std::string& s);
void print_board();
void print_highlighted_board(char player);
void make_move(int row, int col, char player);
bool is_valid_move(int row, int col, char player);
bool is_game_over();
bool turn_skip(char player);
std::pair<int, int> calculate_scores();
void print_scores();
void print_winning_message();
void export_game(const std::vector<std::pair<char, std::string>>& moves, int game_mode, char player_color, int time_limit_b, int time_limit_w, const std::string& start_pos = "", char resigned_by = '\0');