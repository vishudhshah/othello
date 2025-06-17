#pragma once

#include <utility>
#include "constants.hpp"

int get_game_mode();
int get_search_depth();
int get_time_limit();
char get_disk_color();
std::pair<int, int> get_user_input();
void switch_player(char &currentPlayer);
char get_opponent(char player);