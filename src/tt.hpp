#pragma once

#include <cstdint>
#include <atomic>
#include <cstddef>
#include <utility>

// Alpha-beta bound classification for a stored score, relative to the
// (alpha, beta) window the search was actually called with.
enum BoundType : uint8_t { BOUND_EXACT = 0, BOUND_LOWER = 1, BOUND_UPPER = 2 };

// One TT slot: key and payload travel together as a single 128-bit atomic
// unit (lock-free on arm64/x86-64 with cmpxchg16b/CAS emulation), so a
// concurrent reader can never observe a key validated against one position
// with another position's payload. tt_probe() snapshots the whole entry in
// one atomic load; tt_unpack() just decodes the snapshot.
struct TTEntry {
    uint64_t key{0};     // 0 = no valid entry
    uint64_t payload{0}; // packed score/depth/bound/move
};

constexpr int TT_SCORE_MIN = -131071;
constexpr int TT_SCORE_MAX = 131071;

// Fixed-size, power-of-two-sized, always-replace transposition table indexed
// by key & TT_MASK. Deliberately not an unordered_map: avoids hashing twice
// and unbounded memory growth — standard practice for a search TT. The full
// 64-bit key is stored in each slot so a same-slot, different-position
// collision is detected rather than silently returning a wrong result.
constexpr size_t TT_INDEX_BITS = 22; // 2^22 entries, ~134MB at 32B/slot
constexpr size_t TT_SIZE = size_t(1) << TT_INDEX_BITS;
constexpr size_t TT_MASK = TT_SIZE - 1;

void tt_clear();

// Testing/benchmarking only (see main.cpp's --no-tt): globally disables
// probe/store without touching call sites, for an apples-to-apples
// TT-on vs TT-off comparison of the same search.
void tt_set_enabled(bool enabled);

// Probes the slot for `key`. On hit, snapshots the whole entry (key +
// payload) in ONE atomic load and copies it into `*out` — a value copy, so
// no concurrent tt_store can change what the caller reads afterward. The
// search's only caller passes a stack TTEntry: zero heap allocation on the
// probe path. Returns true if the slot held this position (hash-index
// collision = miss), false otherwise.
bool tt_probe(uint64_t key, TTEntry* out);

// Unpacks a successfully probed entry. Call only after a true tt_probe
// result, on the same out-entry. Returns the stored score, depth, bound,
// and move via out-params.
void tt_unpack(const TTEntry* e, int& score, int& depth, uint8_t& bound,
               std::pair<int, int>& move);

// Store into the slot keyed by `key` (always-replace). SMP-safe: the whole
// entry (key + payload) is one atomic 128-bit store, so concurrent readers
// observe either the complete old entry or the complete new one — never a
// key from one position with a payload from another. Scores outside
// [TT_SCORE_MIN, TT_SCORE_MAX] are not cached (the search's win scores are
// ~1e6+; terminal returns never reach tt_store, but this is a hard guard).
void tt_store(uint64_t key, int depth, int score, uint8_t bound, std::pair<int, int> move);
