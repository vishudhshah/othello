#include "bitboard.hpp"

namespace {

constexpr uint64_t NOT_A_FILE = ~0x0101010101010101ULL; // excludes col 0
constexpr uint64_t NOT_H_FILE = ~0x8080808080808080ULL; // excludes col 7

// All 8 directions as (delta_row, delta_col), matching board.cpp's
// direction convention (just reordered to (dr, dc) for clarity here).
constexpr int DIRS[8][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1},
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
};

// Shifts every set bit one square in direction (dr, dc), masking out the
// squares that would otherwise wrap around the left/right board edge into
// the adjacent row (the single most common bitboard-Othello bug). Row
// (top/bottom) overflow needs no masking: it naturally shifts bits off the
// 64-bit register on both ends without wrapping.
inline uint64_t shift_dir(uint64_t b, int dr, int dc) {
    if (dc == 1) b &= NOT_H_FILE;
    else if (dc == -1) b &= NOT_A_FILE;
    int shift = dr * 8 + dc;
    return shift > 0 ? (b << shift) : (b >> (-shift));
}

} // namespace

uint64_t bb_get_moves(uint64_t player_bb, uint64_t opp_bb) {
    uint64_t empty = ~(player_bb | opp_bb);
    uint64_t moves = 0;
    for (auto& d : DIRS) {
        int dr = d[0], dc = d[1];
        // Opponent discs directly adjacent to a player disc in this direction.
        uint64_t candidates = shift_dir(player_bb, dr, dc) & opp_bb;
        while (candidates) {
            uint64_t next = shift_dir(candidates, dr, dc);
            moves |= next & empty;       // run of opponent discs ends on an empty square: legal move
            candidates = next & opp_bb;  // otherwise keep flooding through more opponent discs
        }
    }
    return moves;
}

uint64_t bb_flip_mask(uint64_t player_bb, uint64_t opp_bb, uint64_t move_bit) {
    uint64_t flips = 0;
    for (auto& d : DIRS) {
        int dr = d[0], dc = d[1];
        uint64_t line = 0;
        uint64_t candidate = shift_dir(move_bit, dr, dc);
        while (candidate & opp_bb) {
            line |= candidate;
            candidate = shift_dir(candidate, dr, dc);
        }
        if (candidate & player_bb) {
            flips |= line;
        }
    }
    return flips;
}

std::vector<std::pair<int, int>> bb_to_coords(uint64_t bb) {
    std::vector<std::pair<int, int>> out;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        out.push_back({sq / 8, sq % 8});
        bb &= bb - 1;
    }
    return out;
}

namespace {

inline std::pair<int, int> transform_coord(int r, int c, int t) {
    switch (t) {
        case 0: return {r, c};           // identity
        case 1: return {c, 7 - r};       // rotate 90 cw
        case 2: return {7 - r, 7 - c};   // rotate 180
        case 3: return {7 - c, r};       // rotate 270 cw
        case 4: return {r, 7 - c};       // mirror columns
        case 5: return {c, r};           // transpose
        case 6: return {7 - r, c};       // flip rows
        case 7: return {7 - c, 7 - r};   // anti-transpose
        default: return {r, c};
    }
}

} // namespace

uint64_t bb_apply_transform(uint64_t bb, int t) {
    uint64_t result = 0;
    while (bb) {
        int sq = __builtin_ctzll(bb);
        bb &= bb - 1;
        auto [nr, nc] = transform_coord(sq / 8, sq % 8, t);
        result |= (1ULL << (nr * 8 + nc));
    }
    return result;
}

int bb_inverse_transform(int t) {
    // Every transform is its own inverse except the 90/270 rotation pair.
    static const int inv[8] = {0, 3, 2, 1, 4, 5, 6, 7};
    return inv[t];
}

Canonical bb_canonicalize(uint64_t black_bb, uint64_t white_bb) {
    Canonical best{black_bb, white_bb, 0};
    for (int t = 1; t < 8; t++) {
        uint64_t tb = bb_apply_transform(black_bb, t);
        uint64_t tw = bb_apply_transform(white_bb, t);
        if (tb < best.black || (tb == best.black && tw < best.white)) {
            best = {tb, tw, t};
        }
    }
    return best;
}
