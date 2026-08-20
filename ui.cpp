#include "ui.hpp"
#include "constants.hpp"
#include "board.hpp"
#include "input.hpp"

#include <ncurses.h>
#include <algorithm>
#include <clocale>
#include <csignal>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <deque>
#include <format>
#include <functional>
#include <string_view>

namespace {

// ---- Margin: keep the whole screen off the terminal's top-left corner ----
constexpr int MARGIN_TOP = 1;
constexpr int MARGIN_LEFT = 2;

inline int AY(int row) { return row + MARGIN_TOP; }
inline int AX(int col) { return col + MARGIN_LEFT; }

// ---- Layout constants, derived from the board's fixed-width box layout ----
// Row 0: column header, Row 1: top border, then 8 data rows at stride 2
// (each followed by a separator row), closing border after the last one.
// (All of the following are local/relative coordinates; AY/AX apply the margin.)
// Row prefix is "N " + the box's left edge (3 chars: digit, space, border char).
// Each cell then occupies a 4-char slot: space, glyph, space, separator.
constexpr int CELL_SLOT_START_X = 3;
constexpr int CELL_STRIDE_X = 4;
constexpr int CELL_STRIDE_Y = 2;

constexpr int ROW_STATUS = 0;   // current move number/turn, top of screen
constexpr int ROW_HEADER = 2;   // row 1 left blank as padding above the board
constexpr int ROW_TOP = ROW_HEADER + 1;
constexpr int CELL_ORIGIN_Y = ROW_TOP + 1;
constexpr int ROW_BOTTOM = ROW_TOP + 1 + BOARD_SIZE * 2 - 1;
constexpr int ROW_SCORE = ROW_BOTTOM + 2;   // blank line, then score, below the board
constexpr int ROW_OPENING = ROW_SCORE + 1;  // current opening name, if matched (e.g. "Tiger")
constexpr int ROW_MESSAGE = ROW_OPENING + 1;
constexpr int ROW_LEGEND = ROW_MESSAGE + 1;
constexpr int ROW_LOG_HEADER = ROW_LEGEND + 2; // blank line, then header
constexpr int ROW_LOG_START = ROW_LOG_HEADER + 1;

constexpr size_t MOVE_LOG_VISIBLE = 10; // number of log lines shown at once

constexpr int MIN_COLS = 42;
constexpr int MIN_ROWS = ROW_LOG_START + (int)MOVE_LOG_VISIBLE + 2;

constexpr int PAIR_DISC_NORMAL = 1;
constexpr int PAIR_DISC_LASTMOVE = 2;
constexpr int PAIR_VALID_HINT = 3;
constexpr int PAIR_MENU_SELECTED = 4;

bool g_colors_on = false;
bool g_unicode = false;
char g_last_player = PLAYER1;
std::string g_last_opening_name;

enum class ClickResult { Cell, Outside, Ambiguous };
struct ClickOutcome { ClickResult result; int row; int col; };
volatile sig_atomic_t g_resized = 0;
std::deque<std::string> g_move_log;
int g_log_scroll = 0; // entries scrolled back from the newest (0 = showing the latest)

bool detect_unicode() {
    auto has_utf8 = [](const char* s) -> bool {
        if (!s) return false;
        std::string_view sv(s);
        return sv.find("UTF-8") != std::string_view::npos ||
               sv.find("utf-8") != std::string_view::npos ||
               sv.find("UTF8")  != std::string_view::npos ||
               sv.find("utf8")  != std::string_view::npos;
    };
    return has_utf8(std::getenv("LC_ALL")) ||
           has_utf8(std::getenv("LANG"))   ||
           has_utf8(std::getenv("LC_CTYPE"));
}

void handle_sigwinch(int) { g_resized = 1; }

void handle_signal_exit(int sig) {
    endwin();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

bool terminal_too_small() {
    if (COLS < MIN_COLS || LINES < MIN_ROWS) {
        clear();
        mvprintw(0, 0, "Terminal too small - please resize to at least %dx%d.", MIN_COLS, MIN_ROWS);
        refresh();
        return true;
    }
    return false;
}

void apply_resize_if_needed() {
    if (!g_resized) return;
    g_resized = 0;
    endwin();
    refresh();
    clear();
    render_game_screen(g_last_player, "", g_last_opening_name);
}

ClickOutcome screen_to_cell(int y, int x) {
    int local_y = y - MARGIN_TOP;
    int local_x = x - MARGIN_LEFT;

    // Reject clicks entirely outside the board's bounding rectangle.
    int box_right = CELL_SLOT_START_X + BOARD_SIZE * CELL_STRIDE_X;
    if (local_y < ROW_TOP || local_y > ROW_BOTTOM || local_x < 0 || local_x > box_right) {
        return {ClickResult::Outside, 0, 0};
    }

    // Rows: clamp clicks on the outer border/header (above row 0 or below row 7) to the
    // nearest edge row, so the whole box is clickable. Between two interior data rows,
    // require an exact match — a click exactly on the shared separator line is genuinely
    // ambiguous and should be rejected (prompting a retry) rather than silently assigned
    // to whichever neighbor happens to be picked.
    int row;
    if (local_y <= CELL_ORIGIN_Y) {
        row = 0;
    } else if (local_y >= CELL_ORIGIN_Y + (BOARD_SIZE - 1) * CELL_STRIDE_Y) {
        row = BOARD_SIZE - 1;
    } else {
        int rel = local_y - CELL_ORIGIN_Y;
        if (rel % CELL_STRIDE_Y != 0) return {ClickResult::Ambiguous, 0, 0};
        row = rel / CELL_STRIDE_Y;
    }

    // Columns: same idea — clamp clicks left of column A or right of column H to the
    // nearest edge column, but reject a click exactly on an interior separator character.
    int glyph_col0 = CELL_SLOT_START_X + 1;
    int col;
    if (local_x <= glyph_col0) {
        col = 0;
    } else if (local_x >= glyph_col0 + (BOARD_SIZE - 1) * CELL_STRIDE_X) {
        col = BOARD_SIZE - 1;
    } else {
        int rel = local_x - CELL_SLOT_START_X;
        int within = rel % CELL_STRIDE_X;
        if (within == CELL_STRIDE_X - 1) return {ClickResult::Ambiguous, 0, 0};
        col = rel / CELL_STRIDE_X;
    }

    return {ClickResult::Cell, row, col};
}

void draw_board(char current_player, bool show_hints = true) {
    std::vector<std::pair<int, int>> valid_moves;
    if (show_hints) valid_moves = compute_valid_moves(current_player);
    auto is_hint = [&](int i, int j) {
        for (const auto& p : valid_moves) if (p.first == i && p.second == j) return true;
        return false;
    };

    mvaddstr(AY(ROW_HEADER), AX(0), "    A   B   C   D   E   F   G   H");
    mvaddstr(AY(ROW_TOP), AX(0), g_unicode ? "  ╔═══╤═══╤═══╤═══╤═══╤═══╤═══╤═══╗"
                                        : "  +---+---+---+---+---+---+---+---+");

    const char* sep_line = g_unicode ? "  ╟───┼───┼───┼───┼───┼───┼───┼───╢"
                                     : "  +---+---+---+---+---+---+---+---+";
    const char* bottom_line = g_unicode ? "  ╚═══╧═══╧═══╧═══╧═══╧═══╧═══╧═══╝"
                                        : "  +---+---+---+---+---+---+---+---+";
    const char* v_border = g_unicode ? "║" : "|";
    const char* v_sep = g_unicode ? "│" : "|";

    for (int i = 0; i < BOARD_SIZE; i++) {
        int y = CELL_ORIGIN_Y + i * CELL_STRIDE_Y;
        move(AY(y), AX(0));
        printw("%d ", i + 1);
        addstr(v_border);
        for (int j = 0; j < BOARD_SIZE; j++) {
            addch(' ');
            char c = cell_at(i, j);
            bool last = (last_move.first == i && last_move.second == j);
            bool hint = (c == EMPTY && is_hint(i, j));

            const char* glyph = " ";
            int attrs = A_NORMAL;
            if (c == PLAYER1 || c == PLAYER2) {
                glyph = (c == PLAYER1) ? (g_unicode ? "○" : "B") : (g_unicode ? "●" : "W");
                if (last) attrs = g_colors_on ? (COLOR_PAIR(PAIR_DISC_LASTMOVE) | A_BOLD) : (A_BOLD | A_REVERSE);
                else      attrs = g_colors_on ? (COLOR_PAIR(PAIR_DISC_NORMAL) | A_BOLD)   : A_BOLD;
            } else if (hint) {
                glyph = g_unicode ? "·" : "_";
                attrs = g_colors_on ? COLOR_PAIR(PAIR_VALID_HINT) : A_UNDERLINE;
            }
            attron(attrs);
            addstr(glyph);
            attroff(attrs);
            addch(' ');
            addstr(j < BOARD_SIZE - 1 ? v_sep : v_border);
        }
        if (i < BOARD_SIZE - 1) mvaddstr(AY(y + 1), AX(0), sep_line);
    }
    mvaddstr(AY(ROW_BOTTOM), AX(0), bottom_line);
}

void draw_chrome(char current_player, const std::string& status_line, const std::string& opening_name) {
    move(AY(ROW_STATUS), AX(0)); clrtoeol();
    mvaddstr(AY(ROW_STATUS), AX(0), status_line.empty()
        ? std::format("{}'s turn.", player_name(current_player)).c_str()
        : status_line.c_str());

    auto [b_score, w_score] = calculate_scores();
    move(AY(ROW_SCORE), AX(0)); clrtoeol();
    mvprintw(AY(ROW_SCORE), AX(0), "%s: %d, %s: %d", player_name(PLAYER1).c_str(), b_score, player_name(PLAYER2).c_str(), w_score);

    move(AY(ROW_OPENING), AX(0)); clrtoeol();
    if (!opening_name.empty()) mvaddstr(AY(ROW_OPENING), AX(0), opening_name.c_str());

    move(AY(ROW_MESSAGE), AX(0)); clrtoeol();

    move(AY(ROW_LEGEND), AX(0)); clrtoeol();
    mvaddstr(AY(ROW_LEGEND), AX(0), "Click a cell or type e.g. D3 then Enter  |  U: Undo  |  R: Resign");
}

void draw_move_log() {
    int total = (int)g_move_log.size();
    int max_scroll = std::max(0, total - (int)MOVE_LOG_VISIBLE);
    if (g_log_scroll > max_scroll) g_log_scroll = max_scroll;
    if (g_log_scroll < 0) g_log_scroll = 0;

    move(AY(ROW_LOG_HEADER), AX(0)); clrtoeol();
    std::string header = "Recent moves:";
    if (total > (int)MOVE_LOG_VISIBLE) {
        int end = total - g_log_scroll;              // exclusive, chronological order
        int start = end - (int)MOVE_LOG_VISIBLE;
        header = std::format("Recent moves ({}-{} of {})  |  {} or j/k to scroll",
                              start + 1, end, total, g_unicode ? "↑/↓" : "Up/Down");
    }
    mvaddstr(AY(ROW_LOG_HEADER), AX(0), header.c_str());

    int end = total - g_log_scroll;
    int start = std::max(0, end - (int)MOVE_LOG_VISIBLE);
    int i = 0;
    for (int idx = end - 1; idx >= start; idx--, i++) {
        move(AY(ROW_LOG_START + i), AX(0));
        clrtoeol();
        mvaddstr(AY(ROW_LOG_START + i), AX(0), g_move_log[idx].c_str());
    }
    for (; i < (int)MOVE_LOG_VISIBLE; i++) {
        move(AY(ROW_LOG_START + i), AX(0));
        clrtoeol();
    }
}

int run_menu(const std::string& title, const std::vector<std::string>& options, int default_index = 0) {
    int sel = default_index;
    for (;;) {
        apply_resize_if_needed();
        if (terminal_too_small()) { napms(200); continue; }
        clear();
        mvaddstr(AY(0), AX(0), title.c_str());
        for (size_t i = 0; i < options.size(); i++) {
            int attrs = ((int)i == sel) ? (g_colors_on ? COLOR_PAIR(PAIR_MENU_SELECTED) : A_REVERSE) : A_NORMAL;
            attron(attrs);
            mvaddstr(AY((int)(2 + i)), AX(2), options[i].c_str());
            attroff(attrs);
        }
        mvaddstr(AY((int)(2 + options.size() + 1)), AX(0), "Arrow keys (or j/k) + Enter, click an option, or press its number key.");
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK && (ev.bstate & BUTTON1_PRESSED)) {
                int idx = (ev.y - MARGIN_TOP) - 2;
                if (idx >= 0 && idx < (int)options.size()) return idx;
            }
            continue;
        }
        if (ch == KEY_RESIZE) continue;
        if (ch == KEY_UP || ch == 'k') { sel = (sel - 1 + (int)options.size()) % (int)options.size(); continue; }
        if (ch == KEY_DOWN || ch == 'j') { sel = (sel + 1) % (int)options.size(); continue; }
        if (ch == '\n' || ch == KEY_ENTER) return sel;
        if (ch >= '1' && ch <= '0' + (int)options.size()) return ch - '1';
    }
}

std::string run_text_entry(const std::string& prompt, const std::function<bool(const std::string&)>& validator, const std::string& error_hint) {
    std::string buf;
    std::string error;
    for (;;) {
        apply_resize_if_needed();
        if (terminal_too_small()) { napms(200); continue; }
        clear();
        mvaddstr(AY(0), AX(0), prompt.c_str());
        mvaddstr(AY(2), AX(0), ("> " + buf).c_str());
        if (!error.empty()) mvaddstr(AY(4), AX(0), error.c_str());
        curs_set(1);
        move(AY(2), AX(2 + (int)buf.size()));
        refresh();

        int ch = getch();
        if (ch == KEY_RESIZE) continue;
        if (ch == '\n' || ch == KEY_ENTER) {
            if (!buf.empty() && validator(buf)) { curs_set(0); return buf; }
            error = error_hint;
            continue;
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!buf.empty()) buf.pop_back();
            continue;
        }
        if (ch >= 32 && ch < 127) buf += (char)ch;
    }
}

} // namespace

void ui_init() {
    std::setlocale(LC_ALL, "");
    initscr();

    // ncurses waits up to ESCDELAY ms after a lone ESC byte to see whether more
    // bytes (a mouse report, arrow key, etc.) follow before deciding it was a
    // standalone Escape keypress. set_escdelay() (called after initscr(), which
    // is the reliable way to apply it — the ESCDELAY env var isn't always
    // honored) pushes that wait as close to zero as ncurses allows, since a
    // real click's full byte sequence arrives from the terminal in one burst
    // anyway and doesn't need any wait to be recognized.
    set_escdelay(1);

    g_colors_on = has_colors();
    if (g_colors_on) {
        start_color();
        use_default_colors();
        init_pair(PAIR_DISC_NORMAL, COLOR_WHITE, -1);
        init_pair(PAIR_DISC_LASTMOVE, COLOR_RED, -1);
        init_pair(PAIR_VALID_HINT, COLOR_GREEN, -1);
        init_pair(PAIR_MENU_SELECTED, COLOR_BLACK, COLOR_WHITE);
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    mousemask(BUTTON1_PRESSED, NULL);

    // Without a timeout, getch() blocks until a keypress/click arrives, so a
    // pending SIGWINCH resize sits unapplied until the next input event. A
    // short timeout makes getch() return ERR periodically so the input loops'
    // apply_resize_if_needed() check actually gets to run without user input.
    timeout(100);

    std::signal(SIGINT, handle_signal_exit);
    std::signal(SIGTERM, handle_signal_exit);
    std::signal(SIGWINCH, handle_sigwinch);

    g_unicode = detect_unicode();
}

void ui_teardown() {
    endwin();
}

void log_move(const std::string& text) {
    g_move_log.push_back(text);
    g_log_scroll = 0; // snap back to the newest entry whenever a move is logged
}

void clear_move_log() {
    g_move_log.clear();
    g_log_scroll = 0;
}

void scroll_move_log(int delta) {
    g_log_scroll = std::max(0, g_log_scroll + delta);
    draw_move_log();
    refresh();
}

void discard_pending_input() {
    flushinp();
}

void render_game_screen(char current_player, const std::string& status_line, const std::string& opening_name) {
    g_last_player = current_player;
    g_last_opening_name = opening_name;
    apply_resize_if_needed();
    if (terminal_too_small()) return;
    erase();
    draw_board(current_player);
    draw_chrome(current_player, status_line, opening_name);
    draw_move_log();
    refresh();
}

void render_status_message(const std::string& msg) {
    if (terminal_too_small()) return;
    move(AY(ROW_MESSAGE), AX(0));
    clrtoeol();
    mvaddstr(AY(ROW_MESSAGE), AX(0), msg.c_str());
    refresh();
}

bool render_winning_screen(char resigned_by) {
    apply_resize_if_needed();
    clear();
    draw_board(PLAYER1, /*show_hints=*/false);

    auto [b_score, w_score] = calculate_scores();
    std::string msg;
    if (resigned_by != '\0') {
        char winner = (resigned_by == PLAYER1) ? PLAYER2 : PLAYER1;
        msg = std::format("{} resigned. {} wins!", player_name(resigned_by), player_name(winner));
    } else if (b_score == w_score) {
        msg = "It's a tie!";
    } else {
        char winner = (b_score > w_score) ? PLAYER1 : PLAYER2;
        msg = std::format("{} won by {} points!", player_name(winner), std::abs(b_score - w_score));
    }

    mvprintw(AY(ROW_SCORE), AX(0), "%s: %d, %s: %d", player_name(PLAYER1).c_str(), b_score, player_name(PLAYER2).c_str(), w_score);
    mvaddstr(AY(ROW_MESSAGE), AX(0), ("Game Over. " + msg).c_str());
    mvaddstr(AY(ROW_LEGEND), AX(0), "Press N for a new game, any other key to exit...");
    draw_move_log();
    refresh();

    nodelay(stdscr, FALSE);
    for (;;) {
        int key = getch();
        if (key == KEY_UP || key == 'k') { scroll_move_log(-1); continue; }
        if (key == KEY_DOWN || key == 'j') { scroll_move_log(1); continue; }
        if (key == KEY_PPAGE) { scroll_move_log(-(int)MOVE_LOG_VISIBLE); continue; }
        if (key == KEY_NPAGE) { scroll_move_log((int)MOVE_LOG_VISIBLE); continue; }
        return key == 'n' || key == 'N';
    }
}

// ---- input.hpp definitions (moved here from input.cpp so they can use curses) ----

int get_game_mode() {
    int idx = run_menu("Select game mode:", {
        "1. Player vs Player",
        "2. Player vs AI",
        "3. AI vs AI",
        "4. Puzzle Mode",
    });
    return idx + 1;
}

int get_time_limit(const std::string& label) {
    std::string prompt = std::format("Enter time limit for {} in seconds (0 = default {}s):", label, DEFAULT_TIME_LIMIT);
    std::string s = run_text_entry(prompt, [](const std::string& s) {
        for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
        return true;
    }, "Please enter a non-negative whole number.");
    int v = std::stoi(s);
    return v == 0 ? DEFAULT_TIME_LIMIT : v;
}

char get_disk_color() {
    int idx = run_menu("Choose your disk color:", {"B - Black", "W - White"});
    return idx == 0 ? PLAYER1 : PLAYER2;
}

std::pair<char, std::string> setup_puzzle_board() {
    int fmt = run_menu("Choose board input format:", {
        "1. FEN-like string (e.g. 8/8/8/3WB3/3BW3/8/8/8)",
        "2. 64-character string (e.g. ...........................WB......BW...........................)",
    });

    std::string board_str;
    if (fmt == 0) {
        board_str = run_text_entry("Enter FEN-like board string:",
            [](const std::string& s) { return parse_fen(s); },
            "Invalid FEN string. Rows separated by /, each summing to 8 cells; valid chars: B/b, W/w, 1-8.");
    } else {
        board_str = run_text_entry("Enter 64-character board string:",
            [](const std::string& s) { return parse_64char(s); },
            "Invalid string. Must be exactly 64 characters of B/b, W/w, or '.'.");
    }

    int start_idx = run_menu("Whose turn is it?", {"B - Black", "W - White"});

    for (char& c : board_str) c = std::toupper((unsigned char)c);
    return {start_idx == 0 ? PLAYER1 : PLAYER2, board_str};
}

std::pair<int, int> get_user_input() {
    std::string typed;
    for (;;) {
        apply_resize_if_needed();
        if (terminal_too_small()) { napms(200); continue; }
        int ch = getch();

        if (ch == KEY_MOUSE) {
            MEVENT ev;
            if (getmouse(&ev) == OK && (ev.bstate & BUTTON1_PRESSED)) {
                auto outcome = screen_to_cell(ev.y, ev.x);
                if (outcome.result == ClickResult::Cell) return {outcome.row, outcome.col};
                if (outcome.result == ClickResult::Outside)
                    render_status_message("Click landed outside the board - click a cell.");
                else
                    render_status_message("Click landed between cells - try again.");
            }
            continue;
        }
        if (ch == KEY_RESIZE) continue;

        if (ch == KEY_UP || ch == 'k') { scroll_move_log(-1); continue; }
        if (ch == KEY_DOWN || ch == 'j') { scroll_move_log(1); continue; }
        if (ch == KEY_PPAGE) { scroll_move_log(-(int)MOVE_LOG_VISIBLE); continue; }
        if (ch == KEY_NPAGE) { scroll_move_log((int)MOVE_LOG_VISIBLE); continue; }

        if (typed.empty() && (ch == 'u' || ch == 'U')) return {-1, -1};
        if (typed.empty() && (ch == 'r' || ch == 'R')) return {-2, -2};

        if (typed.empty() && ((ch >= 'A' && ch <= 'A' + BOARD_SIZE - 1) || (ch >= 'a' && ch <= 'a' + BOARD_SIZE - 1))) {
            typed += (char)ch;
            render_status_message(std::format("Move: {} _  (type a digit 1-{})", typed, BOARD_SIZE));
            continue;
        }
        if (typed.size() == 1 && ch >= '1' && ch <= '0' + BOARD_SIZE) {
            typed += (char)ch;
            render_status_message(std::format("Move: {}  (Enter to confirm, Backspace to edit)", typed));
            continue;
        }
        if (typed.size() == 2 && (ch == '\n' || ch == KEY_ENTER)) {
            int row = typed[1] - '1';
            int col = std::toupper((unsigned char)typed[0]) - 'A';
            return {row, col};
        }
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && !typed.empty()) {
            typed.pop_back();
            render_status_message(typed.empty() ? "" : std::format("Move: {} _  (type a digit 1-{})", typed, BOARD_SIZE));
            continue;
        }
        if (ch == 27 && !typed.empty()) {
            typed.clear();
            render_status_message("");
            continue;
        }
        // any other key (including Enter with an incomplete buffer): ignore, keep waiting
    }
}
