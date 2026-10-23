#pragma once

#include <utility>
#include <string>
#include "constants.hpp"

int get_game_mode();
std::pair<char, std::string> setup_puzzle_board(); // returns {starting player, board string entered}
int get_search_depth();
double get_time_limit(const std::string& label = "AI");
// Shared time-limit text parser so the TUI and CLI accept exactly the same
// values: a decimal number of seconds (at most one '.'), minimum
// MIN_TIME_LIMIT. "0" (or "0.0") means "use DEFAULT_TIME_LIMIT". Returns
// false and leaves `out` untouched on invalid input.
bool parse_time_limit(const std::string& s, double& out);
char get_disk_color();
std::pair<int, int> get_user_input();
void switch_player(char &currentPlayer);
char get_opponent(char player);