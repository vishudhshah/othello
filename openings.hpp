#pragma once

#include <cstdint>
#include <string>

// Loads the curated opening database (openings.txt) — parses each named
// opening's move sequence, replaying it with pure local bitboard simulation
// (never touches the live game state, so this is safe to call before any
// game exists), and populates both the move-suggestion book (book.hpp, via
// book_add — every ply of every line, not just endpoints) and the
// name-lookup table this file owns (endpoints only). Clears any previously
// loaded data first, so re-calling never accumulates. Returns false (both
// tables end up empty) if the file is missing or contains no valid
// entries — callers should treat that as "no book/names available," the
// same graceful-degradation contract the old book_load had.
bool openings_load(const std::string& path);

// O(1) lookup: the name of the opening ending exactly at this position
// (canonicalized across all 8 board symmetries), if any. Only exact
// endpoints of curated lines are named, not every intermediate ply — the
// catalogue's own granularity already covers nested sub-lines as separate
// named entries (e.g. "Tiger" and "Mainline Tiger" are both real entries),
// so no name is invented for positions the source data didn't itself name.
bool opening_name_probe(uint64_t black_bb, uint64_t white_bb, char player, std::string& out_name);
