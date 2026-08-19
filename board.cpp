#include "board.hpp"
#include "zobrist.hpp"
#include "bitboard.hpp"
#include <format>
#include <limits>
#include <fstream>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <cstdlib>

std::string player_name(char player) {
    return player == PLAYER1 ? "Black" : "White";
}

char cell_at(int row, int col) {
    int sq = row * BOARD_SIZE + col;
    if ((black_bb >> sq) & 1ULL) return PLAYER1;
    if ((white_bb >> sq) & 1ULL) return PLAYER2;
    return EMPTY;
}

void initialize_board() {
    black_bb = 0;
    white_bb = 0;
    int tl = (BOARD_SIZE/2 - 1) * BOARD_SIZE + (BOARD_SIZE/2 - 1); // top-left of center: PLAYER2
    int tr = (BOARD_SIZE/2 - 1) * BOARD_SIZE + (BOARD_SIZE/2);     // top-right of center: PLAYER1
    int bl = (BOARD_SIZE/2)     * BOARD_SIZE + (BOARD_SIZE/2 - 1); // bottom-left of center: PLAYER1
    int br = (BOARD_SIZE/2)     * BOARD_SIZE + (BOARD_SIZE/2);     // bottom-right of center: PLAYER2
    white_bb |= (1ULL << tl) | (1ULL << br);
    black_bb |= (1ULL << tr) | (1ULL << bl);
    current_hash = compute_hash(black_bb, white_bb);
}

bool parse_fen(const std::string& fen) {
    // Split on '/' and validate exactly 8 rows
    std::vector<std::string> rows;
    std::string current;
    for (char c : fen) {
        if (c == '/') { rows.push_back(current); current.clear(); }
        else           { current += c; }
    }
    rows.push_back(current);

    if ((int)rows.size() != BOARD_SIZE) return false;

    // Parsed into locals and only committed on full success — unlike the
    // pre-bitboard version, a failed parse now leaves the live position
    // untouched instead of partially overwritten.
    uint64_t new_black = 0, new_white = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        int col = 0;
        for (char c : rows[r]) {
            if      (c == 'B' || c == 'b') { if (col >= BOARD_SIZE) return false; new_black |= 1ULL << (r * BOARD_SIZE + col++); }
            else if (c == 'W' || c == 'w') { if (col >= BOARD_SIZE) return false; new_white |= 1ULL << (r * BOARD_SIZE + col++); }
            else if (c >= '1' && c <= '0' + BOARD_SIZE) { int n = c - '0'; if (col + n > BOARD_SIZE) return false; col += n; }
            else return false; // unknown character
        }
        if (col != BOARD_SIZE) return false; // row didn't sum to 8
    }
    black_bb = new_black;
    white_bb = new_white;
    current_hash = compute_hash(black_bb, white_bb);
    return true;
}

bool parse_64char(const std::string& s) {
    if ((int)s.length() != BOARD_SIZE * BOARD_SIZE) return false;

    uint64_t new_black = 0, new_white = 0;
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        char c = s[i];
        if      (c == 'B' || c == 'b') new_black |= 1ULL << i;
        else if (c == 'W' || c == 'w') new_white |= 1ULL << i;
        else if (c == '.')             { /* empty, nothing to set */ }
        else return false; // unknown character
    }
    black_bb = new_black;
    white_bb = new_white;
    current_hash = compute_hash(black_bb, white_bb);
    return true;
}

std::vector<std::pair<int, int>> compute_valid_moves(char player) {
    uint64_t player_bb = (player == PLAYER1) ? black_bb : white_bb;
    uint64_t opp_bb = (player == PLAYER1) ? white_bb : black_bb;
    return bb_to_coords(bb_get_moves(player_bb, opp_bb));
}

MoveUndo make_move_undoable(int row, int col, char player) {
    int player_idx = (player == PLAYER1) ? 0 : 1;
    int opponent_idx = 1 - player_idx;
    uint64_t move_bit = 1ULL << (row * BOARD_SIZE + col);
    uint64_t& player_bb = (player == PLAYER1) ? black_bb : white_bb;
    uint64_t& opp_bb = (player == PLAYER1) ? white_bb : black_bb;

    uint64_t flip_bb = bb_flip_mask(player_bb, opp_bb, move_bit);
    player_bb |= move_bit | flip_bb;
    opp_bb &= ~flip_bb;

    current_hash ^= ZOBRIST_TABLE[player_idx][row * BOARD_SIZE + col];
    uint64_t fb = flip_bb;
    while (fb) {
        int sq = __builtin_ctzll(fb);
        current_hash ^= ZOBRIST_TABLE[opponent_idx][sq];
        current_hash ^= ZOBRIST_TABLE[player_idx][sq];
        fb &= fb - 1;
    }

    return {row, col, player, flip_bb};
}

void make_move(int row, int col, char player) {
    make_move_undoable(row, col, player);
}

void unmake_move(const MoveUndo& undo) {
    int player_idx = (undo.player == PLAYER1) ? 0 : 1;
    int opponent_idx = 1 - player_idx;
    uint64_t move_bit = 1ULL << (undo.row * BOARD_SIZE + undo.col);
    uint64_t& player_bb = (undo.player == PLAYER1) ? black_bb : white_bb;
    uint64_t& opp_bb = (undo.player == PLAYER1) ? white_bb : black_bb;

    player_bb &= ~(move_bit | undo.flip_bb);
    opp_bb |= undo.flip_bb;

    current_hash ^= ZOBRIST_TABLE[player_idx][undo.row * BOARD_SIZE + undo.col];
    uint64_t fb = undo.flip_bb;
    while (fb) {
        int sq = __builtin_ctzll(fb);
        current_hash ^= ZOBRIST_TABLE[opponent_idx][sq];
        current_hash ^= ZOBRIST_TABLE[player_idx][sq];
        fb &= fb - 1;
    }
}

bool is_valid_move(int row, int col, char player) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return false;
    uint64_t player_bb = (player == PLAYER1) ? black_bb : white_bb;
    uint64_t opp_bb = (player == PLAYER1) ? white_bb : black_bb;
    uint64_t moves = bb_get_moves(player_bb, opp_bb);
    return (moves >> (row * BOARD_SIZE + col)) & 1ULL;
}

bool is_game_over() {
    return bb_get_moves(black_bb, white_bb) == 0 && bb_get_moves(white_bb, black_bb) == 0;
}

bool turn_skip(char player) {
    uint64_t player_bb = (player == PLAYER1) ? black_bb : white_bb;
    uint64_t opp_bb = (player == PLAYER1) ? white_bb : black_bb;
    return bb_get_moves(player_bb, opp_bb) == 0;
}

std::pair<int, int> calculate_scores() {
    return std::make_pair(__builtin_popcountll(black_bb), __builtin_popcountll(white_bb));
}

void export_game(const std::vector<std::pair<char, std::string>>& moves, const std::vector<int>& ai_scores, const std::vector<int>& ai_depths, int game_mode, char player_color, int time_limit_b, int time_limit_w, const std::string& start_pos, char resigned_by) {
    // Build timestamped base filename inside logs/ folder
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_info = std::localtime(&t);

    char timestamp[16];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char date_str[20];
    std::strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    std::filesystem::create_directories("logs");
    std::string base = std::format("logs/game_{}", timestamp);

    std::string mode_str = game_mode == 1 ? "Player vs Player"
                         : game_mode == 2 ? "Player vs AI"
                         : game_mode == 3 ? "AI vs AI"
                         :                  "Puzzle Mode";

    auto [b_score, w_score] = calculate_scores();

    // Raw sequence file: moves concatenated on one line
    std::ofstream raw(base + ".log");
    for (const auto& [player, move] : moves) raw << move;
    raw << '\n';

    // Human-readable file
    std::ofstream readable(base + ".txt");
    readable << std::format("Othello Game Log\nDate: {}\nMode: {}\n", date_str, mode_str);
    if (game_mode == 2)
        readable << std::format("Player: {}\nAI time limit: {}s\n", player_name(player_color), time_limit_b);
    else if (game_mode == 3)
        readable << std::format("Black AI time limit: {}s\nWhite AI time limit: {}s\n", time_limit_b, time_limit_w);
    else if (game_mode == 4)
        readable << std::format("Starting position: {}\nAI time limit: {}s\n", start_pos, time_limit_b);
    readable << '\n';

    for (int i = 0; i < (int)moves.size(); i++) {
        if (ai_scores[i] != std::numeric_limits<int>::min())
            readable << std::format("Move {:2}: {} ({}) [score: {}]\n", i + 1, moves[i].second, player_name(moves[i].first), ai_scores[i]);
        else
            readable << std::format("Move {:2}: {} ({})\n", i + 1, moves[i].second, player_name(moves[i].first));
    }
    readable << std::format("\nBlack: {}, White: {}\n", b_score, w_score);
    if (resigned_by != '\0') {
        char winner = (resigned_by == PLAYER1) ? PLAYER2 : PLAYER1;
        readable << std::format("{} resigned, {} wins\n", player_name(resigned_by), player_name(winner));
    } else if (b_score == w_score)
        readable << "Draw\n";
    else if (b_score > w_score)
        readable << std::format("Black won by {} points\n", b_score - w_score);
    else
        readable << std::format("White won by {} points\n", w_score - b_score);

    // Stats CSV: one row per AI move, plus game-level metadata repeated on every row so
    // each file is self-contained for bulk analysis across many games (no join needed).
    std::string player_color_str = (player_color != '\0') ? player_name(player_color) : "";
    // In PvE (mode 2), time_limit_b/w are copies of the same single value and only the
    // AI's side actually uses it — blank the human's side so it doesn't look meaningful.
    std::string time_limit_b_str = (game_mode == 2 && player_color == PLAYER1) ? "" : std::to_string(time_limit_b);
    std::string time_limit_w_str = (game_mode == 2 && player_color == PLAYER2) ? "" : std::to_string(time_limit_w);
    std::ofstream csv(base + ".csv");
    csv << "move_number,score,depth,date,game_mode,player_color,time_limit_b,time_limit_w\n";
    for (int i = 0; i < (int)moves.size(); i++) {
        if (ai_scores[i] != std::numeric_limits<int>::min())
            csv << std::format("{},{},{},{},{},{},{},{}\n", i + 1, ai_scores[i], ai_depths[i],
                date_str, mode_str, player_color_str, time_limit_b_str, time_limit_w_str);
    }
    csv.close();

    // Opt-in chart generation: only runs if OTHELLO_GRAPH is set, and never lets a
    // missing/broken python install print to the terminal (curses is still active here).
    if (std::getenv("OTHELLO_GRAPH")) {
        std::string cmd = std::format("python3 plot_game.py \"{}.csv\" 2>/dev/null", base);
        std::system(cmd.c_str());
    }
}
