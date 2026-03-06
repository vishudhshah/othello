// clang++ -std=c++20 main.cpp constants.cpp board.cpp ai.cpp input.cpp -o main

#include "constants.hpp"
#include "board.hpp"
#include "ai.hpp"
#include "input.hpp"
#include <iostream>
#include <format>
#include <fstream>
#include <ctime>
#include <chrono>

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
    int time_limit_b = DEFAULT_TIME_LIMIT;
    int time_limit_w = DEFAULT_TIME_LIMIT;

    char player_color; // Player's disk color in Player vs AI mode
    string start_pos;  // Starting position string in puzzle mode

    // Get the game mode from the user
    int game_mode = get_game_mode();

    // Start with PLAYER1 (Black); may be overridden by puzzle mode
    char current_player = PLAYER1;

    if (game_mode == 2) {
        // Player vs AI: get the time limit and the player's disk color
        time_limit_b = time_limit_w = get_time_limit();
        player_color = get_disk_color();
    } else if (game_mode == 3) {
        // AI vs AI: get separate time limits for each player
        time_limit_b = get_time_limit("B (Black)");
        time_limit_w = get_time_limit("W (White)");
    } else if (game_mode == 4) {
        // Puzzle mode: load custom board and let AI play both sides
        auto [sp, pos] = setup_puzzle_board();
        current_player = sp;
        start_pos = pos;
        time_limit_b = time_limit_w = get_time_limit();
    }

    // Initialize the standard starting board (skipped in puzzle mode — board already set)
    if (game_mode != 4)
        initialize_board();
    print_highlighted_board(current_player);
    cout << '\n';

    // Initialize the move number
    int move_number = 0;

    // History for undo: each entry stores the board state, active player, move number, and move made before a move
    struct Snapshot { vector<vector<char>> board; char player; int move_num; string move; };
    vector<Snapshot> history;

    // Game loop
    for (;;) {
        // Check if the game is over
        if (is_game_over()) {
            // Print the winning message
            print_winning_message();

            // Export the game log
            vector<pair<char, string>> moves;
            for (const auto& s : history) moves.emplace_back(s.player, s.move);
            char pc = (game_mode == 2) ? player_color : '\0';
            export_game(moves, game_mode, pc, time_limit_b, time_limit_w, start_pos);

            break;
        }

        // Check if the current player's turn should be skipped
        if (turn_skip(current_player)) {
            cout << format("{}'s turn is skipped!\n", player_name(current_player));
            switch_player(current_player);
            print_highlighted_board(current_player);
            continue;
        }

        // Count and print the move number
        move_number++;
        cout << format("Move {}\n", move_number);

        cout << format("{}'s turn.\n", player_name(current_player));

        // Handle different game modes
        if (game_mode == 1 || (game_mode == 2 && current_player == player_color)) {
            // PvP or PvE (Player's turn)
            bool did_undo = false;
            user_input = get_user_input();
            row = user_input.first;
            col = user_input.second;

            while (true) {
                if (row == -1) {
                    // Undo requested
                    if (history.empty()) {
                        cout << "Nothing to undo.\n";
                    } else {
                        // In PvE, pop until we find the player's own snapshot (handles skipped turns)
                        Snapshot restored = history.back();
                        history.pop_back();
                        if (game_mode == 2) {
                            while (!history.empty() && restored.player != player_color) {
                                restored = history.back();
                                history.pop_back();
                            }
                        }
                        board = restored.board;
                        current_player = restored.player;
                        move_number = restored.move_num - 1; // -1 so loop's ++ restores correct number
                        did_undo = true;
                    }
                    print_highlighted_board(current_player);
                    cout << '\n';
                    if (did_undo) break;
                } else if (is_valid_move(row, col, current_player)) {
                    break;
                } else {
                    cout << "Please see the valid moves highlighted!\n";
                }
                user_input = get_user_input();
                row = user_input.first;
                col = user_input.second;
            }

            if (did_undo) continue;
            cout << '\n';
        } else {
            // AI's turn
            int time_limit = (current_player == PLAYER1) ? time_limit_b : time_limit_w;
            pair<int, int> ai_move = predict_move(current_player, time_limit);
            row = ai_move.first;
            col = ai_move.second;

            // Convert row and col to othello notation
            char row_char = row + '1';
            char col_char = col + 'A';
            
            cout << format("AI plays: {}{}\n\n", col_char, row_char);
        }

        // Save board state to history before making the move
        string move_str = {(char)('A' + col), (char)('1' + row)};
        history.push_back({board, current_player, move_number, move_str});

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
