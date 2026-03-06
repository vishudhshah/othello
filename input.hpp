#pragma once

#include <utility>
#include <string>
#include "constants.hpp"

int get_game_mode();
std::pair<char, std::string> setup_puzzle_board(); // returns {starting player, board string entered}
int get_search_depth();
int get_time_limit(const std::string& label = "AI");
char get_disk_color();
std::pair<int, int> get_user_input();
void switch_player(char &currentPlayer);
char get_opponent(char player);