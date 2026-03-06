#include "ai.hpp"
#include "board.hpp"
#include <iostream>
#include <format>
#include <limits>
#include <algorithm>

int game_phase() {
    // Initialize a counter for the total number of discs on the board
    int total_discs = 0;

    // Iterate through all cells in the board and count the number of discs on the board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == PLAYER1 || board[i][j] == PLAYER2) {
                total_discs++;
            }
        }
    }

    // Determine the game phase based on the total number of discs
    switch (total_discs) {
        case 4 ... 20:
            return 1;
        case 21 ... 48:
            return 2;
        case 49 ... 60:
            return 3;
        case 61 ... 64:
            return 4;
        default:
            return 0;
    }
}

// Compute stable discs for a player and write into stable[8][8].
// A disc is stable if it cannot be flipped for the rest of the game.
static void compute_stable(char player, bool stable[8][8]) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            stable[i][j] = false;

    // Pass 1: corners
    const int corners[4][2] = {{0,0},{0,7},{7,0},{7,7}};
    for (auto& c : corners)
        if (board[c[0]][c[1]] == player)
            stable[c[0]][c[1]] = true;

    // Pass 2: edges — propagate from stable corners along each edge
    // Top / bottom rows
    for (int row : {0, 7}) {
        for (int j = 1; j < BOARD_SIZE; j++)
            if (board[row][j] == player && stable[row][j-1])
                stable[row][j] = true;
        for (int j = BOARD_SIZE - 2; j >= 0; j--)
            if (board[row][j] == player && stable[row][j+1])
                stable[row][j] = true;
    }
    // Left / right columns
    for (int col : {0, 7}) {
        for (int i = 1; i < BOARD_SIZE; i++)
            if (board[i][col] == player && stable[i-1][col])
                stable[i][col] = true;
        for (int i = BOARD_SIZE - 2; i >= 0; i--)
            if (board[i][col] == player && stable[i+1][col])
                stable[i][col] = true;
    }

    // Pass 3: interior — fixpoint iteration
    // A disc is stable if, in all 4 axes, it is bounded on both sides by a wall or stable disc.
    static const int axes[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] != player || stable[i][j]) continue;
                bool all_ok = true;
                for (auto& ax : axes) {
                    int dr = ax[0], dc = ax[1];
                    // Check both directions along this axis
                    bool ok_neg = false, ok_pos = false;
                    // negative direction
                    int r = i - dr, c = j - dc;
                    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE) {
                        if (stable[r][c]) { ok_neg = true; break; }
                        r -= dr; c -= dc;
                    }
                    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) ok_neg = true; // hit wall
                    // positive direction
                    r = i + dr; c = j + dc;
                    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE) {
                        if (stable[r][c]) { ok_pos = true; break; }
                        r += dr; c += dc;
                    }
                    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) ok_pos = true; // hit wall
                    if (!ok_neg || !ok_pos) { all_ok = false; break; }
                }
                if (all_ok) { stable[i][j] = true; changed = true; }
            }
        }
    }
}

static int count_stable_discs(char player) {
    bool stable[8][8];
    compute_stable(player, stable);
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            if (stable[i][j]) count++;
    return count;
}

// Returns a correction to position-weight score for danger squares (X/C-squares)
// when the adjacent corner is already owned by piece_player.
static int dynamic_danger_correction(char piece_player) {
    struct DangerSquare { int r, c, cr, cc, base_penalty, corrected_bonus; };
    static const DangerSquare DANGER_SQUARES[] = {
        // X-squares (base -100, bonus +20 if corner is yours)
        {1,1, 0,0, -100, 20}, {1,6, 0,7, -100, 20},
        {6,1, 7,0, -100, 20}, {6,6, 7,7, -100, 20},
        // C-squares (base -10, bonus +5 if corner is yours)
        {0,1, 0,0, -10,  5}, {1,0, 0,0, -10,  5},
        {0,6, 0,7, -10,  5}, {1,7, 0,7, -10,  5},
        {7,1, 7,0, -10,  5}, {6,0, 7,0, -10,  5},
        {7,6, 7,7, -10,  5}, {6,7, 7,7, -10,  5},
    };
    int correction = 0;
    for (const auto& ds : DANGER_SQUARES) {
        if (board[ds.r][ds.c] == piece_player && board[ds.cr][ds.cc] == piece_player) {
            // undo the static penalty and apply a positive bonus
            correction += ds.corrected_bonus - ds.base_penalty;
        }
    }
    return correction;
}

static int count_frontier_discs(char player) {
    static const int dirs[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] != player) continue;
            for (auto& d : dirs) {
                int ni = i + d[0], nj = j + d[1];
                if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE && board[ni][nj] == EMPTY) {
                    count++;
                    break;
                }
            }
        }
    }
    return count;
}

int evaluate_board(char player) {
    int phase = game_phase();
    char opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;

    // Phase 4: raw disc count only
    if (phase == 4) {
        int p1 = 0, p2 = 0;
        for (int i = 0; i < BOARD_SIZE; i++)
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] == PLAYER1) p1++;
                else if (board[i][j] == PLAYER2) p2++;
            }
        return (player == PLAYER1) ? p1 - p2 : p2 - p1;
    }

    const PhaseWeights& w = PHASE_WEIGHTS[phase];

    // Single loop: position weights + mobility counts
    int player1_pos = 0, player2_pos = 0;
    int player_moves = 0, opponent_moves = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                if (is_valid_move(i, j, player))   player_moves++;
                if (is_valid_move(i, j, opponent)) opponent_moves++;
            } else if (board[i][j] == PLAYER1) {
                player1_pos += (phase <= 2) ? POSITION_WEIGHTS[i][j] : ENDGAME_WEIGHTS[i][j];
            } else if (board[i][j] == PLAYER2) {
                player2_pos += (phase <= 2) ? POSITION_WEIGHTS[i][j] : ENDGAME_WEIGHTS[i][j];
            }
        }
    }

    // Dynamic danger correction: undo static penalties for X/C-squares when corner is owned
    player1_pos += dynamic_danger_correction(PLAYER1);
    player2_pos += dynamic_danger_correction(PLAYER2);

    int material = (player == PLAYER1) ? player1_pos - player2_pos : player2_pos - player1_pos;

    // Relative mobility in [-100, +100]
    int total_moves = player_moves + opponent_moves;
    int mobility = (total_moves > 0) ? (100 * (player_moves - opponent_moves)) / total_moves : 0;

    int p1_stable = count_stable_discs(PLAYER1);
    int p2_stable = count_stable_discs(PLAYER2);
    int stability = (player == PLAYER1) ? p1_stable - p2_stable : p2_stable - p1_stable;

    int frontier = 0;
    if (w.frontier != 0) {
        // Fewer frontier discs is better (less vulnerable)
        frontier = count_frontier_discs(opponent) - count_frontier_discs(player);
    }

    return w.material * material
         + w.mobility * mobility
         + w.stability * stability
         + w.frontier * frontier;
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
    int phase = game_phase();
    if (phase == 1 || phase == 2 || phase == 3) {
        // If it possible to end the game early and win then do so
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
    } else if (depth == 0 || is_game_over()) {
        return evaluate_board(player);
    }

    // Initialize the best score
    int best_score = std::numeric_limits<int>::min();

    // Get the sorted valid moves for the current player
    std::vector<std::pair<int, int>> sorted_moves = get_sorted_moves(player);

    // If no valid moves were found, pass the turn to the opponent
    if (sorted_moves.empty()) {
        return -negamax(depth, -beta, -alpha, opponent);
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

    // Get the number of empty cells on the board
    int empty_cells = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                empty_cells++;
            }
        }
    }

    // While time is left and depth is less than the number of empty cells
    while (std::chrono::steady_clock::now() < end_time && current_depth <= empty_cells) {
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
            int score = -negamax(current_depth, std::numeric_limits<int>::min() + 1, std::numeric_limits<int>::max(), opponent);

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