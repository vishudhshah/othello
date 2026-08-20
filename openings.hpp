#pragma once

#include <cstdint>
#include <string>

// Loads the curated opening database (openings.txt) — parses each named
// opening's move sequence, replaying it with pure local bitboard simulation
// (never touches the live game state, so this is safe to call before any
// game exists), and populates both the move-suggestion book (book.hpp, via
// book_add — every ply of every line, not just endpoints) and the
// name-lookup table this file owns (every ply of every line too — see
// opening_name_probe for how in-progress vs. confirmed matches are
// distinguished). Clears any previously loaded data first, so re-calling
// never accumulates. Returns false (both tables end up empty) if the file
// is missing or contains no valid entries — callers should treat that as
// "no book/names available," the same graceful-degradation contract the
// old book_load had.
bool openings_load(const std::string& path);

// O(1) lookup for this position (canonicalized across all 8 board
// symmetries): true and fills out_name if there's something to show, false
// if the position doesn't relate to any curated line (caller should keep
// showing whatever it last displayed, if anything — sticky).
//   - If this position IS some opening's own final ply: out_name is the
//     plain name (e.g. "Tiger") — a confirmed match.
//   - Else if this position is partway through exactly ONE remaining
//     curated line (every other line that ever passed through here has
//     already diverged): out_name is "<Name> category" (e.g. "Wing
//     Variation category") — the game is uniquely heading toward that
//     opening, just hasn't completed its sequence yet. This is what makes
//     the display update after every move instead of only at endpoints.
//   - Else (position is consistent with 2+ different remaining lines, or
//     matches none): returns false — genuinely ambiguous, don't guess.
bool opening_name_probe(uint64_t black_bb, uint64_t white_bb, char player, std::string& out_name);
