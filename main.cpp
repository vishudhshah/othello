// clang++ -std=c++20 main.cpp constants.cpp board.cpp ai.cpp input.cpp -o main

#include "constants.hpp"
#include "board.hpp"
#include "ai.hpp"
#include "input.hpp"
#include "ui.hpp"
#include "zobrist.hpp"
#include "tt.hpp"
#include <format>
#include <limits>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdio>

using namespace std;

// --- Headless CLI mode -----------------------------------------------------
// Non-interactive entry points used for regression testing the engine (perft,
// self-play, single-position search) without going through the ncurses TUI.
// See the plan doc's Phase 0 for rationale; this is the harness every later
// phase (Zobrist/TT/bitboard/book) verifies itself against.

// Full-width node count to a fixed remaining ply depth. A pass (no legal
// moves for the side to move) does not consume a ply, matching negascout's
// own pass convention (ai.cpp) so perft node counts stay meaningful as a
// baseline for that search.
//
// When verify_zobrist is set, asserts current_hash (maintained incrementally
// by make_move) matches a from-scratch compute_hash() at every node visited —
// this is --verify-zobrist's actual check. On mismatch, prints the offending
// position depth and aborts the traversal early (returns 0, caller reports).
//
// Note: this file previously also carried a --verify-bitboard dual-run check
// here, comparing the standalone bitboard.cpp move generator against the
// char-array compute_valid_moves() at every node (Phase 3's parity-testing
// window). That check passed cleanly (perft depths 1-8, plus hand-crafted
// edge-column and near-full-board positions) and was removed once board.cpp
// itself became bitboard-backed — there's no longer a separate char-array
// implementation left to compare against.
static bool zobrist_mismatch_found = false;
static uint64_t perft(int depth, char player, bool verify_zobrist) {
    if (verify_zobrist && !zobrist_mismatch_found && current_hash != compute_hash(black_bb, white_bb)) {
        printf("ZOBRIST MISMATCH at perft depth=%d: current_hash=%llu compute_hash=%llu\n",
            depth, (unsigned long long)current_hash, (unsigned long long)compute_hash(black_bb, white_bb));
        zobrist_mismatch_found = true;
    }
    if (depth == 0) return 1;
    if (is_game_over()) return 1;
    auto moves = compute_valid_moves(player);
    if (moves.empty()) {
        return perft(depth, get_opponent(player), verify_zobrist);
    }
    uint64_t nodes = 0;
    for (auto& mv : moves) {
        MoveUndo undo = make_move_undoable(mv.first, mv.second, player);
        nodes += perft(depth - 1, get_opponent(player), verify_zobrist);
        unmake_move(undo);
    }
    return nodes;
}

// Loads a starting position from --fen/--64 if given, else the standard
// initial position. Returns false (and prints an error) on a bad position string.
static bool load_headless_position(const string& fen, const string& s64) {
    if (!fen.empty()) {
        if (!parse_fen(fen)) { printf("ERROR invalid --fen\n"); return false; }
    } else if (!s64.empty()) {
        if (!parse_64char(s64)) { printf("ERROR invalid --64\n"); return false; }
    } else {
        initialize_board();
    }
    return true;
}

// Fixed-depth root search, bypassing predict_move's time-based IDDFS budget.
// Used only by --search --fixed-depth, for an exact apples-to-apples TT-on
// vs TT-off (--no-tt) comparison at a controlled depth — predict_move's own
// time-boxed search can legitimately reach a different depth run-to-run.
static pair<int, int> search_fixed_depth(char player, int depth, int& out_score) {
    char opponent = get_opponent(player);
    vector<pair<int, int>> moves = get_sorted_moves(player);
    pair<int, int> best_move = moves[0];
    int best_score = numeric_limits<int>::min();
    for (auto& mv : moves) {
        MoveUndo undo = make_move_undoable(mv.first, mv.second, player);
        int score = -negascout(depth - 1, numeric_limits<int>::min() + 1, numeric_limits<int>::max(), opponent);
        unmake_move(undo);
        if (score > best_score) {
            best_score = score;
            best_move = mv;
        }
    }
    out_score = best_score;
    return best_move;
}

// Returns 0/1 (a real exit code) if argv requested a headless mode and it ran;
// returns -1 if no headless flag was present, meaning the normal TUI should start.
static int run_headless(int argc, char** argv) {
    vector<string> args(argv + 1, argv + argc);
    auto has_flag = [&](const string& f) { return find(args.begin(), args.end(), f) != args.end(); };
    auto get_val = [&](const string& f, const string& def = "") -> string {
        auto it = find(args.begin(), args.end(), f);
        if (it != args.end() && next(it) != args.end()) return *next(it);
        return def;
    };

    if (has_flag("--perft")) {
        int depth = stoi(get_val("--perft", "1"));
        if (!load_headless_position(get_val("--fen"), get_val("--64"))) return 1;
        string pl = get_val("--player", "B");
        char player = (pl == "W" || pl == "w") ? PLAYER2 : PLAYER1;
        bool verify_zobrist = has_flag("--verify-zobrist");
        zobrist_mismatch_found = false;
        uint64_t nodes = perft(depth, player, verify_zobrist);
        printf("PERFT depth=%d nodes=%llu\n", depth, (unsigned long long)nodes);
        if (verify_zobrist) printf("ZOBRIST %s\n", zobrist_mismatch_found ? "MISMATCH" : "OK");
        return 0;
    }

    if (has_flag("--selfplay")) {
        int time_limit = stoi(get_val("--time", to_string(DEFAULT_TIME_LIMIT)));
        if (!load_headless_position(get_val("--fen"), get_val("--64"))) return 1;
        string pl = get_val("--player", "B");
        char current_player = (pl == "W" || pl == "w") ? PLAYER2 : PLAYER1;
        int move_number = 0;
        while (!is_game_over()) {
            if (turn_skip(current_player)) {
                current_player = get_opponent(current_player);
                continue;
            }
            move_number++;
            int score, out_depth;
            pair<int, int> mv = predict_move(current_player, time_limit, score, out_depth);
            make_move(mv.first, mv.second, current_player);
            printf("MOVE %d %c %c%d SCORE %d DEPTH %d NODES %llu\n",
                move_number, current_player, (char)('A' + mv.second), mv.first + 1,
                score, out_depth, (unsigned long long)node_count);
            current_player = get_opponent(current_player);
        }
        auto [b_score, w_score] = calculate_scores();
        printf("RESULT BLACK %d WHITE %d\n", b_score, w_score);
        return 0;
    }

    if (has_flag("--search")) {
        if (has_flag("--no-tt")) tt_set_enabled(false);
        if (!load_headless_position(get_val("--fen"), get_val("--64"))) return 1;
        string pl = get_val("--player", "B");
        char player = (pl == "W" || pl == "w") ? PLAYER2 : PLAYER1;

        if (has_flag("--fixed-depth")) {
            int depth = stoi(get_val("--fixed-depth", "1"));
            node_count = 0;
            int score;
            pair<int, int> mv = search_fixed_depth(player, depth, score);
            printf("MOVE %c%d SCORE %d DEPTH %d NODES %llu\n",
                (char)('A' + mv.second), mv.first + 1, score, depth, (unsigned long long)node_count);
            return 0;
        }

        int time_limit = stoi(get_val("--time", to_string(DEFAULT_TIME_LIMIT)));
        int score, out_depth;
        pair<int, int> mv = predict_move(player, time_limit, score, out_depth);
        printf("MOVE %c%d SCORE %d DEPTH %d NODES %llu\n",
            (char)('A' + mv.second), mv.first + 1, score, out_depth, (unsigned long long)node_count);
        return 0;
    }

    return -1;
}

/**
 * @brief Othello game
 *
 * @author Vishudh Shah
 * @since 2024-06-26
 */
int main(int argc, char** argv) {
    init_zobrist_table();
    tt_clear();

    int headless_result = run_headless(argc, argv);
    if (headless_result >= 0) return headless_result;

    ui_init();
    struct UiGuard { ~UiGuard() { ui_teardown(); } } ui_guard;

    // History for undo: each entry stores the board state, active player, move number, and move made before a move
    struct Snapshot { uint64_t black_bb, white_bb, hash; char player; int move_num; string move; int ai_score = numeric_limits<int>::min(); int ai_depth = 0; };

    // Outer loop: each iteration is one full game, from mode selection to game over.
    // play_again controls whether we loop back for a new game or exit after the inner loop.
    bool play_again = true;
    while (play_again) {
        play_again = false;

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
        last_move = {-1, -1};
        clear_move_log();

        // Initialize the move number
        int move_number = 0;

        render_game_screen(current_player, format("{}'s turn.", player_name(current_player)));

        vector<Snapshot> history;

        // Game loop
        for (;;) {
            // Check if the game is over
            if (is_game_over()) {
                // Export the game log
                vector<pair<char, string>> moves;
                vector<int> ai_scores;
                vector<int> ai_depths;
                for (const auto& s : history) { moves.emplace_back(s.player, s.move); ai_scores.push_back(s.ai_score); ai_depths.push_back(s.ai_depth); }
                char pc = (game_mode == 2) ? player_color : '\0';
                export_game(moves, ai_scores, ai_depths, game_mode, pc, time_limit_b, time_limit_w, start_pos);

                // Show the final result and let the player choose to start a new game or exit
                play_again = render_winning_screen();
                break;
            }

            // Check if the current player's turn should be skipped
            if (turn_skip(current_player)) {
                log_move(format("{}'s turn was skipped.", player_name(current_player)));
                switch_player(current_player);
                continue;
            }

            // Count the move number
            move_number++;
            render_game_screen(current_player, format("Move {} - {}'s turn.", move_number, player_name(current_player)));

            // Handle different game modes
            int move_ai_score = numeric_limits<int>::min();
            int move_ai_depth = 0;
            if (game_mode == 1 || (game_mode == 2 && current_player == player_color)) {
                // PvP or PvE (Player's turn)
                bool did_undo = false;
                bool did_resign = false;
                user_input = get_user_input();
                row = user_input.first;
                col = user_input.second;

                while (true) {
                    if (row == -2) {
                        // Resign requested
                        vector<pair<char, string>> moves;
                        vector<int> ai_scores;
                        vector<int> ai_depths;
                        for (const auto& s : history) { moves.emplace_back(s.player, s.move); ai_scores.push_back(s.ai_score); ai_depths.push_back(s.ai_depth); }
                        char pc = (game_mode == 2) ? player_color : '\0';
                        export_game(moves, ai_scores, ai_depths, game_mode, pc, time_limit_b, time_limit_w, start_pos, current_player);
                        play_again = render_winning_screen(current_player);
                        did_resign = true;
                        break;
                    } else if (row == -1) {
                        // Undo requested
                        if (history.empty()) {
                            render_status_message("Nothing to undo.");
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
                            black_bb = restored.black_bb;
                            white_bb = restored.white_bb;
                            current_hash = restored.hash;
                            current_player = restored.player;
                            move_number = restored.move_num - 1; // -1 so loop's ++ restores correct number
                            if (!history.empty()) {
                                const auto& prev = history.back();
                                last_move = {prev.move[1] - '1', prev.move[0] - 'A'};
                            } else {
                                last_move = {-1, -1};
                            }
                            did_undo = true;
                            log_move(format("Undid move {}.", restored.move_num));
                        }
                        if (did_undo) break;
                    } else if (is_valid_move(row, col, current_player)) {
                        break;
                    } else {
                        render_status_message("Please see the valid moves highlighted!");
                    }
                    user_input = get_user_input();
                    row = user_input.first;
                    col = user_input.second;
                }

                if (did_resign) break;
                if (did_undo) continue;

                log_move(format("Move {}: {} played {}{}.", move_number, player_name(current_player), (char)('A' + col), (char)('1' + row)));
            } else {
                // AI's turn
                int time_limit = (current_player == PLAYER1) ? time_limit_b : time_limit_w;
                // Discard any clicks/keys queued up before the AI starts "thinking" (e.g. leftover
                // from the previous turn), then again right after it moves, so nothing typed or
                // clicked during the AI's turn gets silently played as the human's next move.
                discard_pending_input();
                pair<int, int> ai_move = predict_move(current_player, time_limit, move_ai_score, move_ai_depth);
                discard_pending_input();
                row = ai_move.first;
                col = ai_move.second;

                // Convert row and col to othello notation
                char row_char = row + '1';
                char col_char = col + 'A';

                log_move(format("Move {}: AI ({}) played {}{} (score {}, depth {}).",
                    move_number, player_name(current_player), col_char, row_char, move_ai_score, move_ai_depth));
            }

            // Save board state to history before making the move
            string move_str = {(char)('A' + col), (char)('1' + row)};
            history.push_back({black_bb, white_bb, current_hash, current_player, move_number, move_str, move_ai_score, move_ai_depth});

            // Make the move
            last_move = {row, col};
            make_move(row, col, current_player);

            // Switch to the other player after the turn is complete
            switch_player(current_player);
        }
    }

    return 0;
}
