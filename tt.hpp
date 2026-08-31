#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>

// Alpha-beta bound classification for a stored score, relative to the
// (alpha, beta) window the search was actually called with.
enum BoundType : uint8_t { BOUND_EXACT = 0, BOUND_LOWER = 1, BOUND_UPPER = 2 };

struct TTEntry {
    uint64_t key = 0;
    int32_t score = 0;
    int16_t depth = -1; // -1 = empty slot (sentinel, never a real remaining-depth value)
    uint8_t bound = BOUND_EXACT;
    int8_t move_row = -1, move_col = -1;
};

// Fixed-size, power-of-two-sized, always-replace transposition table indexed
// by key & TT_MASK. Deliberately not an unordered_map: avoids hashing twice
// and unbounded memory growth — standard practice for a search TT. The full
// 64-bit key is stored in each slot so a same-slot, different-position
// collision is detected rather than silently returning a wrong result.
constexpr size_t TT_INDEX_BITS = 22; // 2^22 entries, ~100MB at sizeof(TTEntry)
constexpr size_t TT_SIZE = size_t(1) << TT_INDEX_BITS;
constexpr size_t TT_MASK = TT_SIZE - 1;

void tt_clear();

// Testing/benchmarking only (see main.cpp's --no-tt): globally disables
// probe/store without touching call sites, for an apples-to-apples
// TT-on vs TT-off comparison of the same search.
void tt_set_enabled(bool enabled);

// Returns nullptr if the slot is empty or holds a different position
// (hash-index collision) — callers must not assume a non-null result means
// an exact match at the requested depth, only that it's this position.
const TTEntry* tt_probe(uint64_t key);
void tt_store(uint64_t key, int depth, int score, uint8_t bound, std::pair<int, int> move);
