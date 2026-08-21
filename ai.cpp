#include "ai.hpp"
#include "board.hpp"
#include "zobrist.hpp"
#include "tt.hpp"
#include "bitboard.hpp"
#include "book.hpp"
#include <limits>
#include <algorithm>

uint64_t node_count = 0;

// Killer-move and history-heuristic move ordering. Neither changes the
// search's result (negascout still explores the same set of moves, alpha-
// beta pruning is still exact) — they only change what order sibling moves
// are tried in, which is the single biggest lever for cutoff rate beyond
// the TT-move hint. Both are reset at the start of every predict_move()
// call: they capture "what's been working well in *this* search," and
// carrying that across drastically different future positions (later in
// the game) could mislead ordering rather than help it — same reasoning
// that keeps the TT warm across a whole game (it's keyed by exact
// position, so staleness is harmless) while these aren't.
//
// Killers: up to 2 moves per remaining search depth that most recently
// caused a beta cutoff at that depth (classic 2-slot killer table, indexed
// by remaining depth rather than ply-from-root — the simpler, still
// effective convention).
constexpr int MAX_KILLER_DEPTH = 64;
static std::pair<int, int> g_killers[MAX_KILLER_DEPTH][2];

// History: score per (player, destination square), incremented on a beta
// cutoff weighted by depth*depth (deeper cutoffs are stronger signals).
static int g_history[2][BOARD_SIZE * BOARD_SIZE];

static void reset_move_ordering_heuristics() {
    for (auto& slot : g_killers) slot[0] = slot[1] = {-1, -1};
    for (auto& row : g_history) for (int& v : row) v = 0;
}

static void record_cutoff(char player, int depth, int row, int col) {
    int player_idx = (player == PLAYER1) ? 0 : 1;
    g_history[player_idx][row * BOARD_SIZE + col] += depth * depth;

    if (depth >= 0 && depth < MAX_KILLER_DEPTH) {
        std::pair<int, int> mv = {row, col};
        if (g_killers[depth][0] != mv) {
            g_killers[depth][1] = g_killers[depth][0];
            g_killers[depth][0] = mv;
        }
    }
}

int game_phase() {
    int total_discs = __builtin_popcountll(black_bb | white_bb);

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
            if (cell_at(c[0], c[1]) == players[p])
                stable[p][c[0]][c[1]] = true;

    // Pass 2: edges — propagate from stable corners along each edge
    for (int row : {0, BOARD_SIZE-1}) {
        for (int p = 0; p < 2; p++) {
            for (int j = 1; j < BOARD_SIZE; j++)
                if (cell_at(row, j) == players[p] && stable[p][row][j-1]) stable[p][row][j] = true;
            for (int j = BOARD_SIZE - 2; j >= 0; j--)
                if (cell_at(row, j) == players[p] && stable[p][row][j+1]) stable[p][row][j] = true;
        }
    }
    for (int col : {0, BOARD_SIZE-1}) {
        for (int p = 0; p < 2; p++) {
            for (int i = 1; i < BOARD_SIZE; i++)
                if (cell_at(i, col) == players[p] && stable[p][i-1][col]) stable[p][i][col] = true;
            for (int i = BOARD_SIZE - 2; i >= 0; i--)
                if (cell_at(i, col) == players[p] && stable[p][i+1][col]) stable[p][i][col] = true;
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
                    if (cell_at(i, j) != players[p] || stable[p][i][j]) continue;
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
        if (cell_at(ds.r, ds.c) == piece_player && cell_at(ds.cr, ds.cc) == piece_player) {
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
            if (cell_at(i, j) != player) continue;
            for (auto& d : dirs) {
                int ni = i + d[0], nj = j + d[1];
                if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE && cell_at(ni, nj) == EMPTY) {
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
        int p1 = __builtin_popcountll(black_bb);
        int p2 = __builtin_popcountll(white_bb);
        return (player == PLAYER1) ? p1 - p2 : p2 - p1;
    }

    const PhaseWeights& w = PHASE_WEIGHTS[phase];

    // Mobility: popcount of the legal-move bitmask, O(1) instead of the old
    // per-cell is_valid_move scan (which itself would now recompute the same
    // move mask up to 128 times over).
    uint64_t player_bb = (player == PLAYER1) ? black_bb : white_bb;
    uint64_t opp_bb = (player == PLAYER1) ? white_bb : black_bb;
    int player_moves = __builtin_popcountll(bb_get_moves(player_bb, opp_bb));
    int opponent_moves = __builtin_popcountll(bb_get_moves(opp_bb, player_bb));

    // Position weights
    int player1_pos = 0, player2_pos = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            char c = cell_at(i, j);
            if (c == PLAYER1) {
                player1_pos += (phase <= 2) ? POSITION_WEIGHTS[i][j] : ENDGAME_WEIGHTS[i][j];
            } else if (c == PLAYER2) {
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
    node_count++;
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

    // Transposition table probe. Terminal (game-over) and depth==0 leaf
    // returns above are deliberately never cached: the game-over branch's
    // win_score encodes remaining depth (1000000 + depth*100000 + eval), so
    // reusing it from a different depth context would misrepresent how fast
    // the win is — the classic TT+mate-score bug. Only real internal search
    // nodes (which reach this point) are cached.
    int orig_alpha = alpha;
    int orig_beta = beta;
    uint64_t tt_key = current_hash ^ (player == PLAYER2 ? ZOBRIST_TURN : 0);
    std::pair<int, int> tt_move = {-1, -1};

    const TTEntry* entry = tt_probe(tt_key);
    if (entry) {
        tt_move = {entry->move_row, entry->move_col};
        if (entry->depth >= depth) {
            if (entry->bound == BOUND_EXACT) {
                return entry->score;
            } else if (entry->bound == BOUND_LOWER) {
                alpha = std::max(alpha, entry->score);
            } else if (entry->bound == BOUND_UPPER) {
                beta = std::min(beta, entry->score);
            }
            if (alpha >= beta) {
                return entry->score;
            }
        }
    }

    // Move ordering: try the TT-suggested move first (even from a shallower
    // depth, where it's just a good guess rather than a reusable score) —
    // this is usually the single biggest lever for alpha-beta cutoff rate.
    if (tt_move.first >= 0) {
        auto it = std::find(sorted_moves.begin(), sorted_moves.end(), tt_move);
        if (it != sorted_moves.end()) {
            std::iter_swap(sorted_moves.begin(), it);
        }
    }

    // Next, this depth's killer moves (if legal here and not already the
    // TT move) — moves that recently refuted a sibling node are a good bet
    // to refute this one too.
    int ordered_upto = (tt_move.first >= 0) ? 1 : 0;
    if (depth >= 0 && depth < MAX_KILLER_DEPTH) {
        for (auto& killer : g_killers[depth]) {
            if (killer.first < 0 || killer == tt_move) continue;
            auto it = std::find(sorted_moves.begin() + ordered_upto, sorted_moves.end(), killer);
            if (it != sorted_moves.end()) {
                std::iter_swap(sorted_moves.begin() + ordered_upto, it);
                ordered_upto++;
            }
        }
    }

    // Everything else: stable-sort by history-heuristic score (descending),
    // falling back to the existing static square-priority order for ties —
    // a smoother, whole-list-covering signal beyond just the top 2 killers.
    if (ordered_upto < (int)sorted_moves.size()) {
        int player_idx = (player == PLAYER1) ? 0 : 1;
        std::stable_sort(sorted_moves.begin() + ordered_upto, sorted_moves.end(),
            [player_idx](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                return g_history[player_idx][a.first * BOARD_SIZE + a.second]
                     > g_history[player_idx][b.first * BOARD_SIZE + b.second];
            });
    }

    std::pair<int, int> best_move_found = sorted_moves[0];

    // Iterate through all sorted (valid) moves
    for (int idx = 0; idx < (int)sorted_moves.size(); idx++) {
        int i = sorted_moves[idx].first;
        int j = sorted_moves[idx].second;

        // Simulate the move, keeping an undo record instead of copying the board
        MoveUndo undo = make_move_undoable(i, j, player);

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
        unmake_move(undo);

        // Update the best score
        if (score > best_score) {
            best_score = score;
            best_move_found = {i, j};
        }
        alpha = std::max(alpha, score);

        // Perform alpha-beta pruning
        if (alpha >= beta) {
            record_cutoff(player, depth, i, j);
            break;
        }
    }

    uint8_t bound = (best_score <= orig_alpha) ? BOUND_UPPER
                   : (best_score >= orig_beta)  ? BOUND_LOWER
                   :                              BOUND_EXACT;
    tt_store(tt_key, depth, best_score, bound, best_move_found);

    return best_score;
}

std::pair<int, int> predict_move(char player, int time_limit, int& out_score, int& out_depth) {
    node_count = 0;
    reset_move_ordering_heuristics();

    // Opening book probe. Keyed by canonical (symmetry-normalized) position,
    // so this also naturally never fires on an arbitrary puzzle-mode start
    // position (no explicit puzzle-mode flag needed): the book only
    // contains positions reachable from the standard opening, and a
    // hand-entered puzzle position coincidentally matching one of those is
    // not a real risk. is_valid_move is a defense-in-depth check against
    // any residual symmetry-transform bug — the book must never hand back
    // an illegal move.
    std::pair<int, int> book_move;
    int book_score;
    if (book_probe(black_bb, white_bb, player, book_move, book_score) &&
        is_valid_move(book_move.first, book_move.second, player)) {
        out_score = book_score;
        out_depth = -1; // sentinel: move came from the book, not a live search
        return book_move;
    }

    char opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(time_limit);

    // Get the sorted valid moves for the current player
    std::vector<std::pair<int, int>> sorted_moves = get_sorted_moves(player);

    // If there's only one valid move, return it immediately (no search).
    // Still report a real score instead of leaving out_score/out_depth unset:
    // a static evaluation of the resulting position, same as the depth == 0
    // leaf case below, at negligible cost (no recursion into the opponent).
    if (sorted_moves.size() == 1) {
        MoveUndo undo = make_move_undoable(sorted_moves[0].first, sorted_moves[0].second, player);
        out_score = evaluate_board(player, game_phase());
        unmake_move(undo);
        out_depth = 0;
        return sorted_moves[0];
    }

    // Initialize the best move, best score, and current depth
    std::pair<int, int> best_move;
    int best_score = std::numeric_limits<int>::min();
    int current_depth = 1;
    int best_depth = 0;

    // Get the number of empty cells on the board
    int empty_cells = BOARD_SIZE * BOARD_SIZE - __builtin_popcountll(black_bb | white_bb);

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

            // Simulate the move, keeping an undo record instead of copying the board
            MoveUndo undo = make_move_undoable(move.first, move.second, player);

            // Call negascout to predict the score
            int score = -negascout(current_depth, std::numeric_limits<int>::min() + 1, std::numeric_limits<int>::max(), opponent);

            // Undo the move
            unmake_move(undo);

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

            // Try this depth's best move first at the next (deeper) iteration —
            // a strong ordering guess that also gets a real TT-move hint once
            // negascout re-visits the root position at the new depth.
            auto it = std::find(sorted_moves.begin(), sorted_moves.end(), current_best_move);
            if (it != sorted_moves.end() && it != sorted_moves.begin()) {
                std::iter_swap(sorted_moves.begin(), it);
            }
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
