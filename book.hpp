#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>

// Position-keyed, symmetry-canonicalized opening book. Keyed by the
// lexicographically-smallest of a position's 8 symmetric orientations
// (bitboard.hpp's bb_canonicalize) plus side-to-move — not by move sequence
// — so it catches transpositions and rotation/reflection-equivalent lines
// for free. Populated at every program start by openings.cpp's
// openings_load() (parsed from the curated openings.txt, not self-play —
// see that file), then probed by predict_move() before it searches. No
// on-disk cache: re-parsing openings.txt takes microseconds, so there's
// nothing to save/load.

struct BookRecord {
    uint64_t black = 0, white = 0; // canonical-orientation position
    char player = 0;               // side to move
    int8_t move_row = -1, move_col = -1; // move, in the SAME canonical orientation as black/white
    int32_t score = 0;
};

bool book_loaded();
size_t book_size();

// Debug/inspection only (main.cpp's --book-dump): prints every record.
void book_dump_all();

// Probes for (black_bb, white_bb) with `player` to move. On a hit, fills
// out_move (already mapped back to the position's actual orientation, not
// the canonical one) and out_score, and returns true. When multiple
// alternatives were stored for this position, picks uniformly at random
// among them for variety.
bool book_probe(uint64_t black_bb, uint64_t white_bb, char player, std::pair<int, int>& out_move, int& out_score);

// --- Population API (used by openings.cpp's openings_load) ---

// Discards any in-memory book records — openings_load() calls this before
// (re)populating, so re-loading never accumulates duplicate/stale records.
void book_clear();

// Adds one (position, move, score) to the in-memory book, keyed by the
// canonical form of (black_bb, white_bb, player). The move is stored
// transformed into that canonical orientation.
void book_add(uint64_t black_bb, uint64_t white_bb, char player, int row, int col, int score);

// Sorts the in-memory book by key — must be called after all book_add calls
// (book_probe relies on the sorted order).
void book_finalize();

// Used by openings_load() to avoid adding a duplicate record when two lines
// share a prefix and agree on the move there: linear-scans the (possibly
// still-unsorted, mid-population) book for an existing record at this
// position, mapping its move back to the query's actual orientation.
// Returns false if no record exists yet for this position. When one does
// exist but specifies a *different* move, that's not a conflict — it's a
// second legitimate alternative from real opening theory, and the caller
// adds it as its own record (book_probe already picks among alternatives).
bool book_find_move(uint64_t black_bb, uint64_t white_bb, char player, std::pair<int, int>& out_move);
