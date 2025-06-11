#include "input.hpp"
#include <iostream>
#include <limits>
#include <format>
#include <cctype>

int get_game_mode() {
    int mode;

    // Get the mode of the game from the user
    std::cout << "Enter the mode of the game:\n"
        << "1. Player vs Player\n"
        << "2. Player vs AI\n"
        << "3. AI vs AI\n";
    std::cin >> mode;

    // Validate the user input
    while (std::cin.fail() || mode < 1 || mode > 3) {
        std::cin.clear(); // Clear error flags
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore rest of the line
        std::cout << "Please enter a valid game mode.\n";
        std::cin >> mode;
    }
    std::cout << '\n';

    return mode;
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

int get_time_limit() {
    int time_limit;

    // Get the time limit from the user
    std::cout << "Enter the time limit for AI moves (in seconds):\n";
    std::cout << std::format("If you would like to use the default time limit ({} seconds), enter 0.\n", DEFAULT_TIME_LIMIT);
    std::cin >> time_limit;

    // Validate the user input
    while (time_limit < 0) {
        std::cout << "Please enter a valid time limit.\n";
        std::cin >> time_limit;
    }
    std::cout << '\n';

    // Use the default time limit if the user entered 0
    return time_limit == 0 ? DEFAULT_TIME_LIMIT : time_limit;
}

char get_disk_color() {
    char disk_color;

    // Get the disk color for the player from the user
    std::cout << "Enter the disk color for the player (B/b or W/w):\n";
    std::cin >> disk_color;

    // Validate the user input
    while (disk_color != PLAYER1 && disk_color != PLAYER2 && disk_color != 'b' && disk_color != 'w') {
        std::cout << "Please enter a valid disk color (B/b or W/w):\n";
        std::cin >> disk_color;
    }
    std::cout << '\n';

    // Convert to uppercase
    disk_color = std::toupper(disk_color);

    return disk_color;
}

std::pair<int, int> get_user_input() {
    std::string user_input;
    int row, col;

    // Get user input
    std::cout << "Enter your move (eg. A1 or a1): ";
    std::cin >> user_input;

    // Validate the user input
    while (user_input.length() != 2 || user_input[0] < 'A' || (user_input[0] > 'H' && user_input[0] < 'a') || user_input[0] > 'h' || user_input[1] < '1' || user_input[1] > '8') {
        std::cout << "Please enter a valid move (eg. A1 or a1): ";
        std::cin >> user_input;
    }

    // Convert the user input to row and column
    row = user_input[1] - '1';
    col = std::toupper(user_input[0]) - 'A';

    return std::make_pair(row, col);
}

void switch_player(char &currentPlayer) {
    currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;
}

char get_opponent(char player) {
    return (player == PLAYER1) ? PLAYER2 : PLAYER1;
} 