#include "ai.hpp"
#include "board.hpp"
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
        case 4 ... BOARD_SIZE * BOARD_SIZE / 3:
            return 1;
        case BOARD_SIZE * BOARD_SIZE / 3 + 1 ... BOARD_SIZE * BOARD_SIZE * 3 / 4:
            return 2;
        case BOARD_SIZE * BOARD_SIZE * 3 / 4 + 1 ... BOARD_SIZE * BOARD_SIZE - 4:
            return 3;
        case BOARD_SIZE * BOARD_SIZE - 3 ... BOARD_SIZE * BOARD_SIZE:
            return 4;
        default:
            return 0;
    }
}

// Compute stable disc counts for both players in one pass.
// A disc is stable if it cannot be flipped for the rest of the game.
// Returns {player1_stable, player2_stable}.
static std::pair<int,int> count_stable_both() {
    bool stable[2][BOARD_SIZE][BOARD_SIZE] = {};  // [0]=PLAYER1, [1]=PLAYER2
    const char players[2] = {PLAYER1, PLAYER2};

    // Pass 1: corners
    const int corners[4][2] = {{0,0},{0,BOARD_SIZE-1},{BOARD_SIZE-1,0},{BOARD_SIZE-1,BOARD_SIZE-1}};
    for (auto& c : corners)
        for (int p = 0; p < 2; p++)
            if (board[c[0]][c[1]] == players[p])
                stable[p][c[0]][c[1]] = true;

    // Pass 2: edges — propagate from stable corners along each edge
    for (int row : {0, BOARD_SIZE-1}) {
        for (int p = 0; p < 2; p++) {
            for (int j = 1; j < BOARD_SIZE; j++)
                if (board[row][j] == players[p] && stable[p][row][j-1]) stable[p][row][j] = true;
            for (int j = BOARD_SIZE - 2; j >= 0; j--)
                if (board[row][j] == players[p] && stable[p][row][j+1]) stable[p][row][j] = true;
        }
    }
    for (int col : {0, BOARD_SIZE-1}) {
        for (int p = 0; p < 2; p++) {
            for (int i = 1; i < BOARD_SIZE; i++)
                if (board[i][col] == players[p] && stable[p][i-1][col]) stable[p][i][col] = true;
            for (int i = BOARD_SIZE - 2; i >= 0; i--)
                if (board[i][col] == players[p] && stable[p][i+1][col]) stable[p][i][col] = true;
        }
    }

    // Pass 3: interior — fixpoint iteration, both players simultaneously
    // A disc is stable if, in all 4 axes, it is bounded on both sides by a wall or stable disc.
    static const int axes[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                for (int p = 0; p < 2; p++) {
                    if (board[i][j] != players[p] || stable[p][i][j]) continue;
                    bool all_ok = true;
                    for (auto& ax : axes) {
                        int dr = ax[0], dc = ax[1];
                        bool ok_neg = false, ok_pos = false;
                        int r = i - dr, c = j - dc;
                        while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE) {
                            if (stable[p][r][c]) { ok_neg = true; break; }
                            r -= dr; c -= dc;
                        }
                        if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) ok_neg = true;
                        r = i + dr; c = j + dc;
                        while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE) {
                            if (stable[p][r][c]) { ok_pos = true; break; }
                            r += dr; c += dc;
                        }
                        if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) ok_pos = true;
                        if (!ok_neg || !ok_pos) { all_ok = false; break; }
                    }
                    if (all_ok) { stable[p][i][j] = true; changed = true; }
                }
            }
        }
    }

    int c1 = 0, c2 = 0;
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++) {
            c1 += stable[0][i][j];
            c2 += stable[1][i][j];
        }
    return {c1, c2};
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

int evaluate_board(char player, int phase) {
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

    auto [p1_stable, p2_stable] = count_stable_both();
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

int negascout(int depth, int alpha, int beta, char player) {
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

            if (player1_score != player2_score) {
                char winner = (player1_score > player2_score) ? PLAYER1 : PLAYER2;
                // Winning sooner (more of the search depth left unused) matters far more
                // than winning by a bigger margin, so the depth term dominates the eval margin.
                int win_score = 1000000 + depth * 100000 + evaluate_board(winner, phase);
                return (player == winner) ? win_score : -win_score;
            } else {
                return 0;
            }
        } else if (depth == 0) {
            return evaluate_board(player, phase);
        }
    } else if (depth == 0 || is_game_over()) {
        return evaluate_board(player, phase);
    }

    // Initialize the best score
    int best_score = std::numeric_limits<int>::min();

    // Get the sorted valid moves for the current player
    std::vector<std::pair<int, int>> sorted_moves = get_sorted_moves(player);

    // If no valid moves were found, pass the turn to the opponent
    if (sorted_moves.empty()) {
        return -negascout(depth, -beta, -alpha, opponent);
    }

    // Iterate through all sorted (valid) moves
    for (int idx = 0; idx < (int)sorted_moves.size(); idx++) {
        int i = sorted_moves[idx].first;
        int j = sorted_moves[idx].second;

        // Make a copy of the board and simulate the move
        Board board_copy = board;
        make_move(i, j, player);

        // NegaScout: full window for first move, null window probe for the rest
        int score;
        if (idx == 0) {
            score = -negascout(depth - 1, -beta, -alpha, opponent);
        } else {
            score = -negascout(depth - 1, -alpha - 1, -alpha, opponent);
            if (score > alpha && score < beta)
                score = -negascout(depth - 1, -beta, -alpha, opponent);
        }

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

std::pair<int, int> predict_move(char player, int time_limit, int& out_score, int& out_depth) {
    char opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(time_limit);

    // Get the sorted valid moves for the current player
    std::vector<std::pair<int, int>> sorted_moves = get_sorted_moves(player);

    // If there's only one valid move, return it immediately (no search).
    // Still report a real score instead of leaving out_score/out_depth unset:
    // a static evaluation of the resulting position, same as the depth == 0
    // leaf case below, at negligible cost (no recursion into the opponent).
    if (sorted_moves.size() == 1) {
        Board board_copy = board;
        make_move(sorted_moves[0].first, sorted_moves[0].second, player);
        out_score = evaluate_board(player, game_phase());
        board = board_copy;
        out_depth = 0;
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
            Board board_copy = board;
            make_move(move.first, move.second, player);

            // Call negascout to predict the score
            int score = -negascout(current_depth, std::numeric_limits<int>::min() + 1, std::numeric_limits<int>::max(), opponent);

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

    out_score = best_score;
    out_depth = best_depth;
    return best_move;
} 
