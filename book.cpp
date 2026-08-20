#include "book.hpp"
#include "bitboard.hpp"
#include "constants.hpp"
#include <vector>
#include <algorithm>
#include <random>
#include <cstdio>

namespace {

std::vector<BookRecord> g_book;
std::mt19937_64 g_rng(std::random_device{}());

// Ordering used both to sort the book and to binary-search it: by key
// (black, white, player) only — records with equal keys (multiple
// near-optimal moves for the same position) stay adjacent in any relative
// order, which is all book_probe needs to collect them as a contiguous run.
bool key_less(const BookRecord& a, const BookRecord& b) {
    if (a.black != b.black) return a.black < b.black;
    if (a.white != b.white) return a.white < b.white;
    return a.player < b.player;
}

bool same_key(const BookRecord& a, const BookRecord& b) {
    return a.black == b.black && a.white == b.white && a.player == b.player;
}

} // namespace

void book_clear() {
    g_book.clear();
}

void book_add(uint64_t black_bb, uint64_t white_bb, char player, int row, int col, int score) {
    Canonical c = bb_canonicalize(black_bb, white_bb);
    uint64_t move_bit = 1ULL << (row * BOARD_SIZE + col);
    uint64_t canon_move_bit = bb_apply_transform(move_bit, c.transform);
    int canon_sq = __builtin_ctzll(canon_move_bit);

    BookRecord rec;
    rec.black = c.black;
    rec.white = c.white;
    rec.player = player;
    rec.move_row = (int8_t)(canon_sq / BOARD_SIZE);
    rec.move_col = (int8_t)(canon_sq % BOARD_SIZE);
    rec.score = score;
    g_book.push_back(rec);
}

void book_finalize() {
    std::sort(g_book.begin(), g_book.end(), key_less);
}

bool book_loaded() { return !g_book.empty(); }
size_t book_size() { return g_book.size(); }

void book_dump_all() {
    for (const auto& r : g_book) {
        printf("black=%016llx white=%016llx player=%c move=(%d,%d) score=%d\n",
            (unsigned long long)r.black, (unsigned long long)r.white, r.player,
            r.move_row, r.move_col, r.score);
    }
}

bool book_probe(uint64_t black_bb, uint64_t white_bb, char player, std::pair<int, int>& out_move, int& out_score) {
    if (g_book.empty()) return false;

    Canonical c = bb_canonicalize(black_bb, white_bb);
    BookRecord probe_key;
    probe_key.black = c.black;
    probe_key.white = c.white;
    probe_key.player = player;

    auto lo = std::lower_bound(g_book.begin(), g_book.end(), probe_key, key_less);
    if (lo == g_book.end() || !same_key(*lo, probe_key)) return false;
    auto hi = lo;
    while (hi != g_book.end() && same_key(*hi, probe_key)) ++hi;

    // Uniform random pick among the near-optimal alternatives stored for
    // this position — simpler than proportional score-weighting, still
    // delivers move variety in AI-vs-AI play instead of deterministic
    // repetition every game.
    size_t n = (size_t)std::distance(lo, hi);
    size_t idx = (n <= 1) ? 0 : (size_t)(g_rng() % n);
    const BookRecord& rec = *(lo + idx);

    uint64_t canon_move_bit = 1ULL << (rec.move_row * BOARD_SIZE + rec.move_col);
    int inv_t = bb_inverse_transform(c.transform);
    uint64_t actual_move_bit = bb_apply_transform(canon_move_bit, inv_t);
    int actual_sq = __builtin_ctzll(actual_move_bit);
    out_move = {actual_sq / BOARD_SIZE, actual_sq % BOARD_SIZE};
    out_score = rec.score;
    return true;
}

bool book_find_move(uint64_t black_bb, uint64_t white_bb, char player, std::pair<int, int>& out_move) {
    Canonical c = bb_canonicalize(black_bb, white_bb);
    for (const auto& rec : g_book) {
        if (rec.black == c.black && rec.white == c.white && rec.player == player) {
            uint64_t canon_move_bit = 1ULL << (rec.move_row * BOARD_SIZE + rec.move_col);
            int inv_t = bb_inverse_transform(c.transform);
            uint64_t actual_move_bit = bb_apply_transform(canon_move_bit, inv_t);
            int actual_sq = __builtin_ctzll(actual_move_bit);
            out_move = {actual_sq / BOARD_SIZE, actual_sq % BOARD_SIZE};
            return true;
        }
    }
    return false;
}
