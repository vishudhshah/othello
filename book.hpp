#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>
#include <string>

// Position-keyed, symmetry-canonicalized opening book. Keyed by the
// lexicographically-smallest of a position's 8 symmetric orientations
// (bitboard.hpp's transforms) plus side-to-move — not by move sequence —
// so it catches transpositions and rotation/reflection-equivalent lines for
// free. Generated offline (see book_generate, driven by main.cpp's
// --gen-book) against the finished bitboard+Zobrist+TT engine, then loaded
// once at program start and probed by predict_move() before it searches.

struct BookRecord {
    uint64_t black = 0, white = 0; // canonical-orientation position
    char player = 0;               // side to move
    int8_t move_row = -1, move_col = -1; // move, in the SAME canonical orientation as black/white
    int32_t score = 0;
};

// Loads a book previously written by book_save(). Returns false (book stays
// empty) if the file doesn't exist or fails to parse — callers should treat
// that as "no book available," not as an error to surface to the user.
bool book_load(const std::string& path);
bool book_save(const std::string& path);
bool book_loaded();
size_t book_size();

// Probes for (black_bb, white_bb) with `player` to move. On a hit, fills
// out_move (already mapped back to the position's actual orientation, not
// the canonical one) and out_score, and returns true. When multiple
// near-optimal moves were stored for this position (see book_generate's
// epsilon), picks uniformly at random among them for variety.
bool book_probe(uint64_t black_bb, uint64_t white_bb, char player, std::pair<int, int>& out_move, int& out_score);

// --- Generation-only API (used by main.cpp's --gen-book) ---

// Adds one (position, move, score) to the in-memory book, keyed by the
// canonical form of (black_bb, white_bb, player). The move is stored
// transformed into that canonical orientation.
void book_add(uint64_t black_bb, uint64_t white_bb, char player, int row, int col, int score);

// Sorts the in-memory book by key — must be called after all book_add calls
// and before book_save (book_probe relies on the sorted order).
void book_finalize();

// Transposition dedup for generation: canonicalizes (black_bb, white_bb,
// player) and returns true if that canonical position was already marked
// visited (caller should skip re-searching it), false if this call just
// marked it visited for the first time.
bool book_visited_mark(uint64_t black_bb, uint64_t white_bb, char player);
void book_clear_visited();
