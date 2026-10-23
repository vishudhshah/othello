#include "input.hpp"
#include <cctype>
#include <iostream>
#include <format>

bool parse_time_limit(const std::string& s, double& out) {
    if (s.empty()) return false;
    bool seen_dot = false;
    for (char c : s) {
        if (c == '.') {
            if (seen_dot) return false;
            seen_dot = true;
        } else if (!std::isdigit((unsigned char)c)) {
            return false;
        }
    }
    try {
        out = std::stod(s);
    } catch (...) {
        return false;
    }
    if (out == 0.0) {
        out = DEFAULT_TIME_LIMIT;
        return true;
    }
    return out >= MIN_TIME_LIMIT;
}

int get_search_depth() {
    int depth;

    // Get the depth of the search tree from the user
    std::cout << "Enter the depth of the search tree:\n";
    std::cout << std::format("If you would like to use the default search depth ({}), enter 0.\n", DEFAULT_DEPTH);
    std::cin >> depth;

    // Validate the user input
    while (depth < 0) {
        std::cout << "Please enter a valid depth.\n";
        std::cin >> depth;
    }
    std::cout << '\n';

    // Use the default search depth if the user entered 0
    return depth == 0 ? DEFAULT_DEPTH : depth;
}

void switch_player(char &currentPlayer) {
    currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;
}

char get_opponent(char player) {
    return (player == PLAYER1) ? PLAYER2 : PLAYER1;
}