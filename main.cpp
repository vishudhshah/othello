// clang++ -std=c++20 main.cpp constants.cpp board.cpp ai.cpp input.cpp -o main

#include "constants.hpp"
#include "board.hpp"
#include "ai.hpp"
#include "input.hpp"
#include <iostream>
#include <format>

using namespace std;

/**
 * @brief Othello game
 * 
 * @author Vishudh Shah
 * @since 2024-06-26
 */
int main() {
    pair<int, int> user_input;
    int row, col;
    int time_limit = DEFAULT_TIME_LIMIT;

    char player_color; // Player's disk color in Player vs AI mode

    // Get the game mode from the user
    int game_mode = get_game_mode();
    
    // If the game mode involves AI, get the time limit from the user
    if (game_mode == 2 || game_mode == 3) {
        time_limit = get_time_limit();

        // If the game mode is Player vs AI, get the disk color for the player from the user
        if (game_mode == 2) {
            player_color = get_disk_color();
        }
    }

    // Start with PLAYER1 (Black)
    char current_player = PLAYER1;

    // Initialize and print the starting board
    initialize_board();
    print_highlighted_board(current_player);
    cout << '\n';

    // Initialize the move number
    int move_number = 0;

    // Game loop
    for (;;) {
        // Check if the game is over
        if (is_game_over()) {
            // Print the winning message
            print_winning_message();
            break;
        }

        // Check if the current player's turn should be skipped
        if (turn_skip(current_player)) {
            cout << format("Player {}'s turn is skipped!\n", current_player);
            switch_player(current_player);
            print_highlighted_board(current_player);
            continue;
        }

        // Count and print the move number
        move_number++;
        cout << format("Move {}\n", move_number);

        cout << format("Player {}'s turn.\n", current_player);

        // Handle different game modes
        if (game_mode == 1 || (game_mode == 2 && current_player == player_color)) {
            // PvP or PvE (Player's turn)
            user_input = get_user_input();
            row = user_input.first;
            col = user_input.second;

            // Validate the user input
            while (!is_valid_move(row, col, current_player)) {
                cout << "Please see the valid moves highlighted!\n";
                user_input = get_user_input();
                row = user_input.first;
                col = user_input.second;
            }
            cout << '\n';
        } else {
            // AI's turn
            pair<int, int> ai_move = predict_move(current_player, time_limit);
            row = ai_move.first;
            col = ai_move.second;

            // Convert row and col to othello notation
            char row_char = row + '1';
            char col_char = col + 'A';
            
            cout << format("AI plays: {}{}\n\n", col_char, row_char);
        }

        // Make the move
        make_move(row, col, current_player);

        // Switch to the other player after the turn is complete
        switch_player(current_player);

        // Print the board with valid moves highlighted for the next player
        print_highlighted_board(current_player);
        cout << '\n';
    }

    return 0;
}
