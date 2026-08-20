#include "openings.hpp"
#include "book.hpp"
#include "bitboard.hpp"
#include "zobrist.hpp"
#include "constants.hpp"
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cctype>

namespace {

// For a given position, tracks every curated line that passes through it.
// exact_name is set when the position IS some opening's own final ply (the
// confirmed case — display the plain name). prefix_names collects every
// opening (including the exact one, if any) whose sequence merely passes
// through this position on the way to a later ply — when exactly one
// remains, the position uniquely determines where the game is heading even
// though it hasn't arrived yet, so that's displayed as "<Name> category"
// (matching how other Othello apps label an in-progress line). When
// several different openings still share this exact position, it's
// genuinely ambiguous which one (if any) the game is heading toward, so
// nothing is displayed and the caller's own sticky logic keeps showing
// whatever last resolved unambiguously.
struct PositionMatches {
    std::string exact_name;
    std::vector<std::string> prefix_names;
};
std::unordered_map<uint64_t, PositionMatches> g_matches;

// Standard Othello starting position, computed locally — mirrors
// board.cpp's initialize_board() but never touches the live global
// black_bb/white_bb, so replaying a curated sequence during import can't
// disturb whatever game (if any) is currently in progress.
void initial_position(uint64_t& black_bb, uint64_t& white_bb) {
    black_bb = 0;
    white_bb = 0;
    int tl = (BOARD_SIZE / 2 - 1) * BOARD_SIZE + (BOARD_SIZE / 2 - 1);
    int tr = (BOARD_SIZE / 2 - 1) * BOARD_SIZE + (BOARD_SIZE / 2);
    int bl = (BOARD_SIZE / 2)     * BOARD_SIZE + (BOARD_SIZE / 2 - 1);
    int br = (BOARD_SIZE / 2)     * BOARD_SIZE + (BOARD_SIZE / 2);
    white_bb |= (1ULL << tl) | (1ULL << br);
    black_bb |= (1ULL << tr) | (1ULL << bl);
}

// "f5" -> row 4, col 5 (algebraic, case-insensitive). False on malformed input.
bool parse_move(const std::string& tok, int& row, int& col) {
    if (tok.size() != 2) return false;
    char c = (char)std::tolower((unsigned char)tok[0]);
    char r = tok[1];
    if (c < 'a' || c > 'h' || r < '1' || r > '8') return false;
    col = c - 'a';
    row = r - '1';
    return true;
}

// Position+side-to-move key for the name table: canonicalize, then combine
// with the Zobrist hash exactly like the TT/book already do.
uint64_t name_key(uint64_t black_bb, uint64_t white_bb, char player) {
    Canonical c = bb_canonicalize(black_bb, white_bb);
    uint64_t h = compute_hash(c.black, c.white);
    return h ^ (player == PLAYER2 ? ZOBRIST_TURN : 0);
}

} // namespace

bool openings_load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    book_clear();
    g_matches.clear();

    std::string line;
    int line_no = 0;
    int loaded = 0;

    while (std::getline(in, line)) {
        line_no++;

        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#') continue;

        size_t bar = line.find('|');
        if (bar == std::string::npos) {
            fprintf(stderr, "openings.txt:%d: missing '|', skipping line\n", line_no);
            continue;
        }

        std::string name = line.substr(0, bar);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();

        std::istringstream iss(line.substr(bar + 1));
        std::string tok;

        uint64_t black_bb, white_bb;
        initial_position(black_bb, white_bb);
        char player = PLAYER1;
        bool ok = true;

        // Every position visited while replaying this line, ply 0 (before
        // any move) through the final ply — used below to register this
        // opening as a "still possible" match at every one of those
        // positions, not just its endpoint.
        std::vector<uint64_t> ply_keys;
        ply_keys.push_back(name_key(black_bb, white_bb, player));

        while (iss >> tok) {
            int row, col;
            if (!parse_move(tok, row, col)) {
                fprintf(stderr, "openings.txt:%d: bad move '%s' in '%s', skipping opening\n",
                    line_no, tok.c_str(), name.c_str());
                ok = false;
                break;
            }

            uint64_t move_bit = 1ULL << (row * BOARD_SIZE + col);
            uint64_t player_bb = (player == PLAYER1) ? black_bb : white_bb;
            uint64_t opp_bb = (player == PLAYER1) ? white_bb : black_bb;
            uint64_t flip = bb_flip_mask(player_bb, opp_bb, move_bit);
            if (flip == 0) {
                fprintf(stderr, "openings.txt:%d: illegal move '%s' in '%s', skipping opening\n",
                    line_no, tok.c_str(), name.c_str());
                ok = false;
                break;
            }

            // Different named openings routinely share a prefix and then
            // diverge — that's not a data error, it's two legitimate
            // alternative continuations from the same position (exactly
            // what book_probe's multi-record-per-key random pick already
            // supports). Only skip adding when this exact move is already
            // recorded for this position, to avoid pure duplicate records
            // when many lines share a long common prefix.
            std::pair<int, int> existing;
            if (!book_find_move(black_bb, white_bb, player, existing) ||
                existing.first != row || existing.second != col) {
                book_add(black_bb, white_bb, player, row, col, 0);
            }

            player_bb |= move_bit | flip;
            opp_bb &= ~flip;
            if (player == PLAYER1) { black_bb = player_bb; white_bb = opp_bb; }
            else                   { white_bb = player_bb; black_bb = opp_bb; }

            player = (player == PLAYER1) ? PLAYER2 : PLAYER1;
            ply_keys.push_back(name_key(black_bb, white_bb, player));
        }

        if (!ok) continue;

        // Register this opening as a possible match at every position it
        // passes through, and as the confirmed (exact) match at its own
        // final position.
        for (uint64_t key : ply_keys) {
            g_matches[key].prefix_names.push_back(name);
        }
        g_matches[ply_keys.back()].exact_name = name;
        loaded++;
    }

    book_finalize();
    return loaded > 0;
}

bool opening_name_probe(uint64_t black_bb, uint64_t white_bb, char player, std::string& out_name) {
    auto it = g_matches.find(name_key(black_bb, white_bb, player));
    if (it == g_matches.end()) return false;
    const PositionMatches& m = it->second;

    if (!m.exact_name.empty()) {
        out_name = m.exact_name;
        return true;
    }
    if (m.prefix_names.size() == 1) {
        out_name = m.prefix_names[0] + " category";
        return true;
    }
    return false;
}
