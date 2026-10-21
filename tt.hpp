#pragma once

#include <cstdint>
#include <atomic>
#include <cstddef>
#include <utility>

// Alpha-beta bound classification for a stored score, relative to the
// (alpha, beta) window the search was actually called with.
enum BoundType : uint8_t { BOUND_EXACT = 0, BOUND_LOWER = 1, BOUND_UPPER = 2 };

// One TT slot. The payload (score/depth/bound/move) is packed into a single
// 32-bit word and read/written as ONE atomic unit together with the key's
// validity bit — see the locking-free SMP protocol in tt.cpp. Layout:
//   bits  0-17 : score (signed, offset-binary, fits +-131071)
//   bits 18-23 : depth (0..63)
//   bits 24-25 : bound
//   bits 26-30 : move row (0..7) -> stored as row+1 so 0 means "no move"
//   bits 31    : valid flag
struct TTEntry {
    std::atomic<uint64_t> word{0}; // key in high 32 bits, payload in low 32
};

constexpr int TT_SCORE_MIN = -131071;
constexpr int TT_SCORE_MAX = 131071;

// Fixed-size, power-of-two-sized, always-replace transposition table indexed
// by key & TT_MASK. Deliberately not an unordered_map: avoids hashing twice
// and unbounded memory growth — standard practice for a search TT. The full
// 64-bit key is stored in each slot so a same-slot, different-position
// collision is detected rather than silently returning a wrong result.
constexpr size_t TT_INDEX_BITS = 22; // 2^22 entries, ~67MB at 16B/slot
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

// Unpacks a successfully probed entry. Call only with a non-null tt_probe
// result. Returns the stored score, depth, bound, and move via out-params.
void tt_unpack(const TTEntry* e, int& score, int& depth, uint8_t& bound,
               std::pair<int, int>& move);

// Store into the slot keyed by `key` (always-replace). SMP-safe: the entire
// slot is one atomic 64-bit store, so concurrent readers observe either the
// old entry or the new one — never a torn mix. Scores outside
// [TT_SCORE_MIN, TT_SCORE_MAX] are not cached (the search's win scores are
// ~1e6+; terminal returns never reach tt_store, but this is a hard guard).
void tt_store(uint64_t key, int depth, int score, uint8_t bound, std::pair<int, int> move);
