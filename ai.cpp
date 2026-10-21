#include "ai.hpp"
#include "board.hpp"
#include "zobrist.hpp"
#include "tt.hpp"
#include "bitboard.hpp"
#include "book.hpp"
#include <limits>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>

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
// Number of search threads the next predict_move() should use. Default 4
// (user decision): 4 independent IDDFS workers over one shared TT. 1 = plain
// single-threaded search on the calling thread (the historical behavior, and
// the --threads 1 escape hatch). Set from main.cpp's --threads flag;
// capped at MAX_SEARCH_THREADS.
int search_threads = 4;
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
    adjacent |= (empty & NOT_A) >> 1;   // right of an empty (source not on col H)
    adjacent |= (empty & NOT_H) << 1;   // left of an empty (source not on col A)
    adjacent |= (empty & NOT_A) >> 9;   // down-right
    adjacent |= (empty & NOT_H) >> 7;   // down-left
    adjacent |= (empty & NOT_A) << 7;   // up-right
    adjacent |= (empty & NOT_H) << 9;   // up-left
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

    TTEntry entry;
    if (tt_probe(tt_key, &entry)) {
        int e_score, e_depth; uint8_t e_bound;
        tt_unpack(&entry, e_score, e_depth, e_bound, tt_move);
        if (e_depth >= depth) {
            if (e_bound == BOUND_EXACT) {
                return e_score;
            } else if (e_bound == BOUND_LOWER) {
                alpha = std::max(alpha, e_score);
            } else if (e_bound == BOUND_UPPER) {
                beta = std::min(beta, e_score);
            }
            if (alpha >= beta) {
                return e_score;
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

    // Everything else: sort by history-heuristic score (descending), falling
    // back to the existing static square-priority order for ties (insertion
    // sort is stable) — a smoother, whole-list-covering signal beyond just
    // the top 2 killers. Insertion sort, not std::stable_sort: n is at most
    // 64, insertion is faster at this size, and stable_sort allocates a
    // temporary buffer on every call (one malloc per search node).
    if (ordered_upto < sorted_count) {
        int player_idx = (player == PLAYER1) ? 0 : 1;
        auto key = [player_idx](const std::pair<int, int>& m) {
            return g_history[player_idx][m.first * BOARD_SIZE + m.second];
        };
        for (std::pair<int, int>* i = sorted_moves + ordered_upto + 1; i != moves_end; ++i) {
            std::pair<int, int> v = *i;
            std::pair<int, int>* j = i;
            while (j != sorted_moves + ordered_upto && key(v) > key(*(j - 1))) {
                *j = *(j - 1);
                --j;
            }
            *j = v;
        }
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

// One worker's IDDFS: identical logic to the single-threaded predict_move
// body (which this function also serves as for nthreads == 1), running on
// the caller's own board copy (thread_local black_bb/white_bb/current_hash —
// seeded by the spawner). Results are reported through `out` at completion
// of each depth; the spawner races workers against each other and picks the
// best completed iteration available.
struct SmpResult {
    std::pair<int, int> move = {-1, -1};
    int score = std::numeric_limits<int>::min();
    int depth = 0;
    uint64_t nodes = 0;
};

static void smp_worker(char player,
                       std::chrono::steady_clock::time_point end_time,
                       uint64_t seed_bb_black, uint64_t seed_bb_white,
                       uint64_t seed_hash, SmpResult& out) {
    // Seed this thread's board copy from the spawner's board. For nthreads==1
    // the spawner passes its own (already-current) values, making these
    // assignments no-ops.
    black_bb = seed_bb_black;
    white_bb = seed_bb_white;
    current_hash = seed_hash;
    reset_move_ordering_heuristics();
    // Per-move node count: zero this thread's counter before searching. For
    // the nthreads==1 path this thread is the persistent main/UI thread, so
    // without this the reported count would accumulate across every AI move
    // of the game instead of counting just this turn's search.
    node_count = 0;

    char opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;

    // Arm this thread's soft deadline (thread_local; read-only during search).
    search_deadline = end_time;
    search_aborted = false;

    std::pair<int, int> sorted_moves[MAX_MOVES];
    int sorted_count = get_sorted_moves(player, sorted_moves);
    std::pair<int, int>* moves_end = sorted_moves + sorted_count;

    SmpResult local;
    int current_depth = 1;
    int empty_cells = BOARD_SIZE * BOARD_SIZE - __builtin_popcountll(black_bb | white_bb);

    while (std::chrono::steady_clock::now() < end_time && current_depth <= empty_cells) {
        std::pair<int, int> current_best_move;
        int current_best_score = std::numeric_limits<int>::min();
        bool completed_depth = true;

        for (int idx = 0; idx < sorted_count; idx++) {
            const auto& move = sorted_moves[idx];
            if (search_aborted || std::chrono::steady_clock::now() >= end_time) {
                completed_depth = false;
                break;
            }
            MoveUndo undo = make_move_undoable(move.first, move.second, player);
            // Depth accounting mirrors the single-threaded driver: the root
            // move is already made, so remaining depth is current_depth - 1
            // (timed-mode depth == fixed-depth depth; see commit 1e182ad).
            int score = -negascout(current_depth - 1, std::numeric_limits<int>::min() + 1,
                                   std::numeric_limits<int>::max(), opponent);
            unmake_move(undo);
            // The deadline can fire inside the LAST root move's subtree: the
            // for loop then ends without revisiting the pre-move check, which
            // would publish an aborted (garbage-scored) iteration as complete.
            // Re-check after every child before using its score.
            if (search_aborted) {
                completed_depth = false;
                break;
            }
            if (score > current_best_score) {
                current_best_move = move;
                current_best_score = score;
            }
        }

        if (!completed_depth) break;

        // Publish this depth's completed result. Plain struct write: the
        // spawner reads these only after joining this thread (or after a
        // mutex-protected handoff), never mid-flight.
        local.move = current_best_move;
        local.score = current_best_score;
        local.depth = current_depth;

        // Iteration-to-iteration best-move-first ordering (same as the
        // single-threaded driver).
        auto it = std::find(sorted_moves, moves_end, current_best_move);
        if (it != moves_end && it != sorted_moves) {
            std::iter_swap(sorted_moves, it);
        }
        current_depth++;
    }

    search_deadline = {};
    search_aborted = false;
    local.nodes = node_count;
    out = local;
}

std::pair<int, int> predict_move(char player, int time_limit, int& out_score, int& out_depth) {
    // Instant paths below (book / forced move) return before any search
    // runs; zero the counter here so they report 0 nodes for THIS turn
    // instead of the previous searched turn's count.
    node_count = 0;

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

    // Forced single move: no search either way.
    {
        std::pair<int, int> probe[MAX_MOVES];
        int probe_count = get_sorted_moves(player, probe);
        if (probe_count == 1) {
            out_score = 0;
            out_depth = 0;
            return probe[0];
        }
    }

    // Resolve the thread count: main.cpp sets search_threads from --threads;
    // clamp to something sane for the hardware (leave 2 cores for the OS/UI)
    // and to the compile-time cap.
    int nthreads = search_threads;
    {
        unsigned hw = std::thread::hardware_concurrency();
        if (hw >= 4) nthreads = std::min(nthreads, (int)hw - 2);
        nthreads = std::clamp(nthreads, 1, MAX_SEARCH_THREADS);
    }

    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(time_limit);

    if (nthreads == 1) {
        // Historical path: search on this thread, board already in place.
        SmpResult r;
        smp_worker(player, end_time, black_bb, white_bb, current_hash, r);
        node_count = 0; // this thread's own count; single-threaded semantics
        // (smp_worker already reset/accumulated into this thread's
        // thread_local node_count; report the total through out params below.)
        uint64_t total = r.nodes;
        node_count = total; // expose via the same global the CLI prints
        out_score = r.score;
        out_depth = r.depth;
        return r.move;
    }

    // Lazy SMP: nthreads independent IDDFS workers over one shared TT.
    // Board copies are seeded from the main thread's board BEFORE spawning
    // (thread_local assignment below runs in the worker, reading seeds
    // captured here). Each worker races the others; the shared TT makes them
    // collaborate — a subtree one worker finishes may be a TT hit for
    // another. Results are collected under a mutex as workers complete each
    // depth; the best completed depth at deadline wins.
    std::vector<std::thread> threads;
    std::vector<SmpResult> results(nthreads);
    std::mutex results_mutex;
    uint64_t seed_black = black_bb, seed_white = white_bb, seed_hash = current_hash;

    for (int t = 0; t < nthreads; t++) {
        threads.emplace_back([&, t]() {
            SmpResult local;
            smp_worker(player, end_time, seed_black, seed_white, seed_hash, local);
            std::lock_guard<std::mutex> lock(results_mutex);
            results[t] = local;
        });
    }
    for (auto& th : threads) th.join();

    // Pick the deepest completed iteration across workers (all workers ran
    // the same IDDFS schedule, so depth is comparable; among equal depths
    // any worker's result is valid — they differ only in ordering luck).
    const SmpResult* best = nullptr;
    uint64_t total_nodes = 0;
    for (const auto& r : results) {
        total_nodes += r.nodes;
        if (!best || r.depth > best->depth ||
            (r.depth == best->depth && r.score > best->score)) {
            best = &r;
        }
    }
    node_count = total_nodes;
    out_score = best->score;
    out_depth = best->depth;
    return best->move;

}
