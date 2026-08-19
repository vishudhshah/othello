#pragma once

#include <cstdint>
#include <utility>
#include <vector>

// Pure bitboard Othello move generation / move application, operating only
// on two uint64_t masks (the moving player's discs, the opponent's discs) —
// no dependency on board.hpp's char[8][8] representation, by design: this
// lets the Phase 3 dual-run parity test compare this implementation against
// the existing char-array logic before board.cpp is switched over to use it
// as the primary representation.
//
// Square indexing matches zobrist.hpp's convention: square = row*8 + col,
// bit 0 = (row 0, col 0).

// Legal-move mask: bit set at every empty square where `player_bb` can
// legally place a disc against `opp_bb`.
uint64_t bb_get_moves(uint64_t player_bb, uint64_t opp_bb);

// Mask of every opponent disc that flips if `player_bb` plays at the single
// set bit in `move_bit`. Does not itself validate that `move_bit` is a legal
// move (mirrors is_valid_move/make_move's separation of concerns).
uint64_t bb_flip_mask(uint64_t player_bb, uint64_t opp_bb, uint64_t move_bit);

// Bit-scan helper: every set bit in `bb` as a (row, col) pair, ascending
// square index. Used for parity-testing against compute_valid_moves() and,
// later, for iterating flipped squares without a nested loop.
std::vector<std::pair<int, int>> bb_to_coords(uint64_t bb);

// The 8 symmetries of a square board (the dihedral group D4): 4 rotations
// times {identity, mirror}. Transform indices:
//   0 = identity          4 = mirror (flip columns)
//   1 = rotate 90 cw       5 = transpose (mirror + rotate90)
//   2 = rotate 180         6 = flip rows (mirror + rotate180)
//   3 = rotate 270 cw      7 = anti-transpose (mirror + rotate270)
// Used by book.cpp to canonicalize a position across all 8 orientations so
// the opening book's position keys catch transpositions-by-symmetry, not
// just transpositions-by-move-order. Not a hot-path operation (book lookups
// happen once per predict_move() call, not per search node), so this is a
// straightforward per-bit remap rather than a SWAR bit-permutation trick.
uint64_t bb_apply_transform(uint64_t bb, int t);

// The transform that undoes `t` (t composed with its inverse is identity).
int bb_inverse_transform(int t);
