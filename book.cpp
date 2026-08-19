#include "book.hpp"
#include "bitboard.hpp"
#include "constants.hpp"
#include <vector>
#include <set>
#include <tuple>
#include <algorithm>
#include <fstream>
#include <random>
#include <cstdio>

namespace {

std::vector<BookRecord> g_book;
std::set<std::tuple<uint64_t, uint64_t, char>> g_visited;
std::mt19937_64 g_rng(std::random_device{}());

struct Canon { uint64_t black, white; int transform; };

Canon canonicalize(uint64_t black_bb, uint64_t white_bb) {
    Canon best{black_bb, white_bb, 0};
    for (int t = 1; t < 8; t++) {
        uint64_t tb = bb_apply_transform(black_bb, t);
        uint64_t tw = bb_apply_transform(white_bb, t);
        if (tb < best.black || (tb == best.black && tw < best.white)) {
            best = {tb, tw, t};
        }
    }
    return best;
}

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
    Canon c = canonicalize(black_bb, white_bb);
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

bool book_save(const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    uint64_t count = g_book.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    if (count > 0) out.write(reinterpret_cast<const char*>(g_book.data()), (std::streamsize)(count * sizeof(BookRecord)));
    return (bool)out;
}

bool book_load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    uint64_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in) return false;
    std::vector<BookRecord> loaded(count);
    if (count > 0) {
        in.read(reinterpret_cast<char*>(loaded.data()), (std::streamsize)(count * sizeof(BookRecord)));
        if (!in) return false;
    }
    g_book = std::move(loaded);
    return true;
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

    Canon c = canonicalize(black_bb, white_bb);
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

bool book_visited_mark(uint64_t black_bb, uint64_t white_bb, char player) {
    Canon c = canonicalize(black_bb, white_bb);
    auto key = std::make_tuple(c.black, c.white, player);
    if (g_visited.count(key)) return true;
    g_visited.insert(key);
    return false;
}

void book_clear_visited() {
    g_visited.clear();
}
