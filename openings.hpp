#pragma once

#include <cstdint>
#include <string>

// Loads the curated opening database (openings.txt) — parses each named
// opening's move sequence, replaying it with pure local bitboard simulation
// (never touches the live game state, so this is safe to call before any
// game exists), and populates both the move-suggestion book (book.hpp, via
// book_add — every ply of every line, not just endpoints) and the
// name-lookup table this file owns (each opening's final position only —
// see opening_name_probe). Clears any previously loaded data first, so
// re-calling never accumulates. Returns false (both tables end up empty)
// if the file is missing or contains no valid entries — callers should
// treat that as "no book/names available," the same graceful-degradation
// contract the old book_load had.
bool openings_load(const std::string& path);

// O(1) lookup for this position (canonicalized across all 8 board
// symmetries): true and fills out_name only if this position IS some
// opening's own final ply (e.g. "Tiger", or a junction entry like
// "Tiger Category" that openings.txt defines explicitly). Otherwise
// returns false — positions partway through a line match nothing, and the
// caller should keep showing whatever it last displayed, if anything
// (sticky).
bool opening_name_probe(uint64_t black_bb, uint64_t white_bb, char player, std::string& out_name);
