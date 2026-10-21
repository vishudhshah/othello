#include "ai.hpp"
#include "board.hpp"
#include "zobrist.hpp"
#include "tt.hpp"
#include "bitboard.hpp"
#include "book.hpp"
#include <limits>
#include <algorithm>

uint64_t thread_local node_count = 0;

// Soft deadline + abort flag for time-boxed searches (see ai.hpp). Default
// state = no deadline, never aborted, so fixed-depth benchmark searches are
// completely unaffected.
//
// SMP: search_deadline is written once by predict_move before workers spawn
// and never mutated during the search, so workers can read it without a lock.
// search_aborted is per-thread (thread_local): each worker aborts and unwinds
// its own stack independently.
thread_local std::chrono::steady_clock::time_point search_deadline{};
thread_local bool search_aborted = false;

// --- Lazy SMP configuration ---
// Number of search threads the next predict_move() should use. 1 = plain
// single-threaded search on the calling thread (the historical behavior, and
// the default until --threads is parsed). Set from main.cpp's --threads flag;
// capped at MAX_SEARCH_THREADS.
int search_threads = 1;
// Generous upper bound: hardware with more cores than this would need a
// different engine design anyway. predict_move clamps the actual count.
constexpr int MAX_SEARCH_THREADS = 16;

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
static thread_local std::pair<int, int> g_killers[MAX_KILLER_DEPTH][2];

// History: score per (player, destination square), incremented on a beta
// cutoff weighted by depth*depth (deeper cutoffs are stronger signals).
// thread_local for Lazy SMP: ordering heuristics are per-thread "what's been
// working in my search" state; sharing them across racing threads would only
// add contention for no quality gain.
static thread_local int g_history[2][BOARD_SIZE * BOARD_SIZE];

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

// Sound local stability detection using bitboards. A disc is protected on
// each axis by either a completely occupied line, the board boundary, or an
// immediately adjacent stable disc of its own color. This avoids searching
// future games; some discs stable only because of global move constraints
// can remain unrecognized.
static std::pair<uint64_t, uint64_t> stable_discs_both() {
    constexpr uint64_t A = 0x0101010101010101ULL;
    constexpr uint64_t H = 0x8080808080808080ULL;
    constexpr uint64_t AB = 0x0303030303030303ULL;
    constexpr uint64_t GH = 0xc0c0c0c0c0c0c0c0ULL;
    constexpr uint64_t ABCD = 0x0f0f0f0f0f0f0f0fULL;
    constexpr uint64_t EFGH = 0xf0f0f0f0f0f0f0f0ULL;
    constexpr uint64_t ROW_EDGES = 0xff000000000000ffULL;
    constexpr uint64_t EDGES = A | H | ROW_EDGES;

    // Spread empties across each complete row/column/diagonal using shifts
    // of 1, 2, and 4 squares. The complement identifies lines with no empty
    // square, which cannot receive a future move along that axis. File masks
    // prevent shifts from wrapping between rows.
    uint64_t empty = ~(black_bb | white_bb);
    uint64_t horizontal = empty;
    horizontal |= ((horizontal & ~H) << 1) | ((horizontal & ~A) >> 1);
    horizontal |= ((horizontal & ~GH) << 2) | ((horizontal & ~AB) >> 2);
    horizontal |= ((horizontal & ~EFGH) << 4) | ((horizontal & ~ABCD) >> 4);
    uint64_t vertical = empty;
    vertical |= (vertical << 8) | (vertical >> 8);
    vertical |= (vertical << 16) | (vertical >> 16);
    vertical |= (vertical << 32) | (vertical >> 32);
    uint64_t diagonal = empty;
    diagonal |= ((diagonal & ~H) << 9) | ((diagonal & ~A) >> 9);
    diagonal |= ((diagonal & ~GH) << 18) | ((diagonal & ~AB) >> 18);
    diagonal |= ((diagonal & ~EFGH) << 36) | ((diagonal & ~ABCD) >> 36);
    uint64_t anti_diagonal = empty;
    anti_diagonal |= ((anti_diagonal & ~A) << 7) | ((anti_diagonal & ~H) >> 7);
    anti_diagonal |= ((anti_diagonal & ~AB) << 14) | ((anti_diagonal & ~GH) >> 14);
    anti_diagonal |= ((anti_diagonal & ~ABCD) << 28) | ((anti_diagonal & ~EFGH) >> 28);

    const uint64_t horizontal_safe = ~horizontal | A | H;
    const uint64_t vertical_safe = ~vertical | ROW_EDGES;
    const uint64_t diagonal_safe = ~diagonal | EDGES;
    const uint64_t anti_diagonal_safe = ~anti_diagonal | EDGES;
    auto find_stable = [&](uint64_t discs) {
        uint64_t stable = discs & horizontal_safe & vertical_safe
                                & diagonal_safe & anti_diagonal_safe;
        for (;;) {
            uint64_t next = discs
                & (horizontal_safe | ((stable & ~H) << 1) | ((stable & ~A) >> 1))
                & (vertical_safe | (stable << 8) | (stable >> 8))
                & (diagonal_safe | ((stable & ~H) << 9) | ((stable & ~A) >> 9))
                & (anti_diagonal_safe | ((stable & ~A) << 7) | ((stable & ~H) >> 7));
            if (next == stable) return stable;
            stable = next;
        }
    };
    return {find_stable(black_bb), find_stable(white_bb)};
}

static std::pair<int,int> count_stable_both() {
    auto [black, white] = stable_discs_both();
    return {__builtin_popcountll(black), __builtin_popcountll(white)};
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
    // A disc is a frontier disc iff at least one of its 8 neighbors is empty.
    // Shift the empties bitboard one step in each direction (masked to prevent
    // row wraparound) and union: the result marks every square adjacent to an
    // empty. One AND with the player's bitboard + popcount gives the answer,
    // replacing the old 64-cell x 8-neighbor scan.
    uint64_t empty = ~(black_bb | white_bb);
    constexpr uint64_t NOT_A = 0xfefefefefefefefeULL; // ~col 0
    constexpr uint64_t NOT_H = 0x7f7f7f7f7f7f7f7fULL; // ~col 7
    uint64_t adjacent = 0;
    adjacent |= empty >> 8;             // below an empty
    adjacent |= empty << 8;             // above an empty
    adjacent |= (empty & NOT_H) >> 1;   // right of an empty
    adjacent |= (empty & NOT_A) << 1;   // left of an empty
    adjacent |= (empty & NOT_H) >> 9;   // down-right
    adjacent |= (empty & NOT_A) >> 7;   // down-left
    adjacent |= (empty & NOT_H) << 7;   // up-right
    adjacent |= (empty & NOT_A) << 9;   // up-left
    uint64_t discs = (player == PLAYER1) ? black_bb : white_bb;
    return __builtin_popcountll(discs & adjacent);
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

    // Position weights: instead of scanning all 64 cells, popcount each
    // disc color's bitboard against a precomputed mask per distinct weight
    // value. The weights tables have few distinct values (5 in
    // POSITION_WEIGHTS, 4 in ENDGAME_WEIGHTS), so this is ~9 popcounts vs a
    // 64-cell branchy scan.
    int player1_pos = 0, player2_pos = 0;
    {
        // (weight, mask) pairs for the phase's table, built once on first use
        static const std::vector<std::pair<int, uint64_t>> pos_masks = [] {
            std::vector<std::pair<int, uint64_t>> m;
            for (int i = 0; i < BOARD_SIZE; i++)
                for (int j = 0; j < BOARD_SIZE; j++) {
                    int w = POSITION_WEIGHTS[i][j];
                    auto it = std::find_if(m.begin(), m.end(),
                        [w](const auto& p) { return p.first == w; });
                    if (it != m.end()) it->second |= 1ULL << (i * BOARD_SIZE + j);
                    else m.emplace_back(w, 1ULL << (i * BOARD_SIZE + j));
                }
            return m;
        }();
        static const std::vector<std::pair<int, uint64_t>> end_masks = [] {
            std::vector<std::pair<int, uint64_t>> m;
            for (int i = 0; i < BOARD_SIZE; i++)
                for (int j = 0; j < BOARD_SIZE; j++) {
                    int w = ENDGAME_WEIGHTS[i][j];
                    auto it = std::find_if(m.begin(), m.end(),
                        [w](const auto& p) { return p.first == w; });
                    if (it != m.end()) it->second |= 1ULL << (i * BOARD_SIZE + j);
                    else m.emplace_back(w, 1ULL << (i * BOARD_SIZE + j));
                }
            return m;
        }();
        const auto& masks = (phase <= 2) ? pos_masks : end_masks;
        for (const auto& [weight, mask] : masks) {
            player1_pos += weight * __builtin_popcountll(black_bb & mask);
            player2_pos += weight * __builtin_popcountll(white_bb & mask);
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

// Legal moves for `player` in static square-priority order (corners first,
// X-squares last). Fills `out` and returns the count. Runs once per search
// node, so it deliberately avoids heap allocation: a fresh vector here cost
// one malloc per node (~2-3M/sec during a real search).
int get_sorted_moves(char player, std::pair<int, int>* out) {
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

    // Compute the legal-move bitmask once and bit-test it in square-priority
    // order, instead of calling is_valid_move() up to 64 times — each of
    // those calls would independently recompute this same bitmask from
    // scratch even though the board doesn't change between checks.
    uint64_t player_bb = (player == PLAYER1) ? black_bb : white_bb;
    uint64_t opp_bb = (player == PLAYER1) ? white_bb : black_bb;
    uint64_t moves_bb = bb_get_moves(player_bb, opp_bb);

    int count = 0;
    for (const auto& move : order) {
        int sq = move.first * BOARD_SIZE + move.second;
        if ((moves_bb >> sq) & 1ULL) {
            out[count++] = move;
        }
    }

    return count;
}

int negascout(int depth, int alpha, int beta, char player) {
    node_count++;

    // Soft-deadline check: abort the moment the time budget is gone. The flag
    // unwinds every caller; nothing is written to the TT after it is raised
    // (see the tt_store guard below), and predict_move() discards the whole
    // iteration's results, so the abort never leaks a bad score into a move
    // decision or a benchmark. The check is one clock read per node — cheap
    // relative to the node's own work.
    if (search_deadline != std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() >= search_deadline) {
        search_aborted = true;
        return 0;
    }

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
    std::pair<int, int> sorted_moves[MAX_MOVES];
    int sorted_count = get_sorted_moves(player, sorted_moves);
    std::pair<int, int>* moves_end = sorted_moves + sorted_count;

    // If no valid moves were found, pass the turn to the opponent
    if (sorted_count == 0) {
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
        auto it = std::find(sorted_moves, moves_end, tt_move);
        if (it != moves_end) {
            std::iter_swap(sorted_moves, it);
        }
    }

    // Next, this depth's killer moves (if legal here and not already the
    // TT move) — moves that recently refuted a sibling node are a good bet
    // to refute this one too.
    int ordered_upto = (tt_move.first >= 0) ? 1 : 0;
    if (depth >= 0 && depth < MAX_KILLER_DEPTH) {
        for (auto& killer : g_killers[depth]) {
            if (killer.first < 0 || killer == tt_move) continue;
            auto it = std::find(sorted_moves + ordered_upto, moves_end, killer);
            if (it != moves_end) {
                std::iter_swap(sorted_moves + ordered_upto, it);
                ordered_upto++;
            }
        }
    }

    // Everything else: stable-sort by history-heuristic score (descending),
    // falling back to the existing static square-priority order for ties —
    // a smoother, whole-list-covering signal beyond just the top 2 killers.
    if (ordered_upto < sorted_count) {
        int player_idx = (player == PLAYER1) ? 0 : 1;
        std::stable_sort(sorted_moves + ordered_upto, moves_end,
            [player_idx](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                return g_history[player_idx][a.first * BOARD_SIZE + a.second]
                     > g_history[player_idx][b.first * BOARD_SIZE + b.second];
            });
    }

    std::pair<int, int> best_move_found = sorted_moves[0];

    // Iterate through all sorted (valid) moves
    for (int idx = 0; idx < sorted_count; idx++) {
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

    // Abort unwind: never let a dead subtree's score into the TT — a stored
    // bound derived from aborted children is garbage (children returned 0
    // because of the clock, not because the position scores 0).
    if (search_aborted) return best_score;

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

    // Arm the in-search soft deadline (see negascout). Without this, a single
    // wide subtree could run arbitrarily past the budget because the loop
    // below only checks the clock between root moves.
    search_deadline = end_time;
    search_aborted = false;

    // Get the sorted valid moves for the current player
    std::pair<int, int> sorted_moves[MAX_MOVES];
    int sorted_count = get_sorted_moves(player, sorted_moves);
    std::pair<int, int>* moves_end = sorted_moves + sorted_count;

    // If there's only one valid move, return it immediately (no search).
    // Deliberately report score 0: a static evaluation would be misleading
    // when no search was performed for this forced move.
    if (sorted_count == 1) {
        // Disarm the deadline here too: this path skips the normal disarm at
        // the end of the IDDFS loop, and a stale armed deadline could abort
        // an unrelated later search (e.g. a fixed-depth benchmark) that runs
        // after this clock time.
        search_deadline = {};
        search_aborted = false;
        out_score = 0;
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

        // Try each move at the current depth (only the first sorted_count
        // entries are real moves — the array is a fixed 64-slot buffer).
        for (int idx = 0; idx < sorted_count; idx++) {
            const auto& move = sorted_moves[idx];

            // Check if we've run out of time before starting a new move
            if (search_aborted || std::chrono::steady_clock::now() >= end_time) {
                // If we have, then we did not complete the depth
                completed_depth = false;
                break;
            }

            // Simulate the move, keeping an undo record instead of copying the board
            MoveUndo undo = make_move_undoable(move.first, move.second, player);

            // Call negascout to predict the score.
            //
            // Depth accounting: the root move is already made, so passing
            // current_depth - 1 makes the reported "depth" count plies BELOW
            // the root move — the same convention as --search --fixed-depth
            // (search_fixed_depth passes depth - 1 after making the root
            // move). Before this alignment, timed mode's "depth D" silently
            // meant fixed-depth mode's "depth D+1" (one ply deeper), which
            // made CSV/log depth numbers incomparable across the two paths.
            // Same tree is searched as before; only the reported number
            // changes (D -> D-1).
            int score = -negascout(current_depth - 1, std::numeric_limits<int>::min() + 1, std::numeric_limits<int>::max(), opponent);

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
            // current_depth here counts plies below the root move (the root
            // move itself is not counted — see the negascout call above), so
            // reporting it directly keeps timed-mode depth equal to
            // fixed-depth mode's for the same search.
            best_depth = current_depth;
            best_score = current_best_score;
            best_move = current_best_move;
        } else {
            // If we didn't complete the depth, we ran out of time
            break;
        }

        // Try this depth's best move first at the next (deeper) iteration —
        // a strong ordering guess that also gets a real TT-move hint once
        // negascout re-visits the root position at the new depth.
        auto it = std::find(sorted_moves, moves_end, best_move);
        if (it != moves_end && it != sorted_moves) {
            std::iter_swap(sorted_moves, it);
        }

        // Increment depth for next iteration
        current_depth++;
    }

    // Disarm the deadline so a later fixed-depth benchmark search (or any
    // other caller of negascout outside predict_move) is never aborted by a
    // stale clock from this call.
    search_deadline = {};
    search_aborted = false;

    out_score = best_score;
    out_depth = best_depth;
    return best_move;
} 
