#include "ai.hpp"
#include "board.hpp"
#include <iostream>
#include <format>
#include <limits>
#include <algorithm>

int game_phase() {
    // Initialize a counter for the total number of pieces on the board
    int total_pieces = 0;

    // Iterate through all cells in the board and count the number of pieces on the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == PLAYER1 || board[i][j] == PLAYER2) {
                total_pieces++;
            }
        }
    }

    // Determine the game phase based on the total number of pieces
    switch (total_pieces) {
        case 4 ... 20:
            return 1;
        case 21 ... 50:
            return 2;
        case 51 ... 60:
            return 3;
        case 61 ... 64:
            return 4;
        default:
            return 0;
    }
}

int evaluate_board(char player) {
    // Initialize counters
    int player1_score = 0;
    int player2_score = 0;
    int num_valid_moves = 0;

    // Determine the game phase
    int phase = game_phase();

    // Iterate through all cells in the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            // Count the number of valid moves for the opponent (for mobility)
            if (board[i][j] == EMPTY && is_valid_move(i, j, player)) {
                num_valid_moves++;
            }

            // Calculate the score for each player based on the position weights and game phase
            if (board[i][j] == PLAYER1) {
                switch (phase) {
                    case 1:
                        player1_score += POSITION_WEIGHTS[i][j];
                        break;
                    case 2:
                        player1_score += POSITION_WEIGHTS[i][j];
                        break;
                    case 3:
                        player1_score += ENDGAME_WEIGHTS[i][j];
                        break;
                    case 4:
                        player1_score += 1;
                        break;
                    default:
                        break;
                }
            } else if (board[i][j] == PLAYER2) {
                switch (phase) {
                    case 1:
                        player2_score += POSITION_WEIGHTS[i][j];
                        break;
                    case 2:
                        player2_score += POSITION_WEIGHTS[i][j];
                        break;
                    case 3:
                        player2_score += ENDGAME_WEIGHTS[i][j];
                        break;
                    case 4:
                        player2_score += 1;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // Calculate the material score and mobility score for the current player
    int material_score = player == PLAYER1 ? player1_score - player2_score : player2_score - player1_score;
    int mobility_score = 10 * num_valid_moves;

    // Return the final evaluation score for the current player
    return material_score + mobility_score;
}

std::vector<std::pair<int, int>> get_sorted_moves(char player) {
    // Define order to check for valid moves
    static const std::vector<std::pair<int, int>> order = {
        // Corners
        {0,0}, {0,7}, {7,0}, {7,7},

        // Sides
        {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, // top
        {7,1}, {7,2}, {7,3}, {7,4}, {7,5}, {7,6}, // bottom
        {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, // left
        {1,7}, {2,7}, {3,7}, {4,7}, {5,7}, {6,7}, // right

        // Outer ring
        {1,2}, {1,3}, {1,4}, {1,5},
        {6,2}, {6,3}, {6,4}, {6,5},
        {2,1}, {3,1}, {4,1}, {5,1},
        {2,6}, {3,6}, {4,6}, {5,6},

        // Inner ring
        {2,2}, {2,3}, {2,4}, {2,5},
        {3,2}, {3,5},
        {4,2}, {4,5},
        {5,2}, {5,3}, {5,4}, {5,5},

        // X
        {1,1}, {1,6}, {6,1}, {6,6},
    };

    std::vector<std::pair<int, int>> valid_moves;
    for (const auto& move : order) {
        if (is_valid_move(move.first, move.second, player)) {
            valid_moves.push_back(move);
        }
    }

    return valid_moves;
}

int negamax(int depth, int alpha, int beta, char player) {
    char opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;

    // Base case: game is over or depth limit reached
    // Handle base case differently based game phase
    int phase = game_phase();
    if (phase == 1 || phase == 2) {
        if (is_game_over()) {
            // Calculate the scores and determine the winner
            std::pair<int, int> scores = calculate_scores();
            int player1_score = scores.first;
            int player2_score = scores.second;

            if (player1_score > player2_score) {
                return (player == PLAYER1) ? (1000000 + evaluate_board(player)) : -(1000000 + evaluate_board(opponent));
            } else if (player2_score > player1_score) {
                return (player == PLAYER2) ? (1000000 + evaluate_board(player)) : -(1000000 + evaluate_board(opponent));
            } else {
                return 0;
            }
        } else if (depth == 0) {
            return evaluate_board(player);
        }
    } else if (phase == 3 || phase == 4) {
        if (depth == 0 || is_game_over()) {
            return evaluate_board(player);
        }
    }

    // Initialize the best score
    int best_score = std::numeric_limits<int>::min();

    // Get the sorted valid moves for the current player
    std::vector<std::pair<int, int>> sorted_moves = get_sorted_moves(player);

    // If no valid moves were found, pass the turn to the opponent
    if (sorted_moves.empty()) {
        return -negamax(depth - 1, -beta, -alpha, opponent);
    }

    // Iterate through all sorted (valid) moves
    for (const auto& move : sorted_moves) {
        int i = move.first;
        int j = move.second;

        // Make a copy of the board and simulate the move
        std::vector<std::vector<char>> board_copy = board;
        make_move(i, j, player);

        // Recursively update the score by calling negamax for the opponent
        int score = -negamax(depth - 1, -beta, -alpha, opponent);

        // Undo the move
        board = board_copy;

        // Update the best score
        best_score = std::max(best_score, score);
        alpha = std::max(alpha, score);

        // Perform alpha-beta pruning
        if (alpha >= beta) {
            break;
        }
    }

    return best_score;
}

std::pair<int, int> predict_move(char player, int time_limit) {
    char opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(time_limit);

    // Get the sorted valid moves for the current player
    std::vector<std::pair<int, int>> sorted_moves = get_sorted_moves(player);

    // If there's only one valid move, return it immediately
    if (sorted_moves.size() == 1) {
        return sorted_moves[0];
    }

    // Initialize the best move, best score, and current depth
    std::pair<int, int> best_move;
    int best_score = std::numeric_limits<int>::min();
    int current_depth = 1;
    int best_depth = 0;

    // While time is left
    while (std::chrono::steady_clock::now() < end_time) {
        // Initialize the current best move and score
        std::pair<int, int> current_best_move;
        int current_best_score = std::numeric_limits<int>::min();

        // Initialize a flag to check if all moves were explored at current depth
        bool completed_depth = true;

        // Try each move at the current depth
        for (const auto& move : sorted_moves) {
            // Check if we've run out of time before starting a new move
            if (std::chrono::steady_clock::now() >= end_time) {
                // If we have, then we did not complete the depth
                completed_depth = false;
                break;
            }

            // Make a copy of the board and simulate the move
            std::vector<std::vector<char>> board_copy = board;
            make_move(move.first, move.second, player);

            // Call negamax to predict the score
            int score = -negamax(current_depth, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), opponent);

            // Undo the move
            board = board_copy;

            // Update the current best move and score
            if (score > current_best_score) {
                current_best_move = move;
                current_best_score = score;
            }
        }

        // Update the best move and score if we explored all moves at this depth
        if (completed_depth) {
            best_depth = current_depth;
            best_score = current_best_score;
            best_move = current_best_move;
        } else {
            // If we didn't complete the depth, we ran out of time
            break;
        }

        // Increment depth for next iteration
        current_depth++;
    }

    std::cout << std::format("Best score for {}: {} (depth reached: {})\n", player, best_score, best_depth);
    return best_move;
} 