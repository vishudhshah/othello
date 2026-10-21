#include "tt.hpp"
#include <vector>
#include <atomic>
#include <new>

// SMP safety (Lazy SMP), final design: one 128-bit atomic per slot carrying
// key AND payload together. A reader that validates a key can never read a
// payload from a different entry — the pair is snapshotted in a single
// atomic operation (lock-free on arm64 and x86-64-via-cmpxchg16b; where the
// platform lacks lock-free 16B atomics std::atomic<Entry> falls back to an
// internal lock, which preserves correctness at some cost).
//
// The previous two-atomic-words design (separate key/payload with
// invalidate -> store payload -> publish key) was NOT correct: a reader that
// validated the key could observe another worker's payload written between
// its key load and its payload load. Reproduced against this exact code
// under a same-slot writer: ~71k wrong scores per ~1M probe-hits. Memory
// ordering alone cannot repair a torn two-word pair; only making the pair
// one atomic unit (or locking the slot) does.
struct Slot {
    std::atomic<TTEntry> entry{};
};
static std::vector<Slot> slots(TT_SIZE);
static bool tt_enabled = true;

void tt_clear() {
    // Single-threaded context (program startup, between searches).
    for (auto& s : slots) {
        s.entry.store(TTEntry{0, 0}, std::memory_order_relaxed);
    }
}

void tt_set_enabled(bool enabled) {
    tt_enabled = enabled;
}

// Payload layout (64 bits):
//   bits 63-32 : score (int32, exact — no clamping needed)
//   bits 31-26 : depth (0..63)
//   bits 25-24 : bound
//   bits 23-19 : move row+1 (0 = none)
//   bits 18-14 : move col+1 (0 = none)
//   bits 13-0  : reserved (0)
static inline uint64_t pack_payload(int score, int depth, uint8_t bound,
                                    std::pair<int, int> move) {
    uint64_t p = 0;
    p |= (uint64_t)(uint32_t)(int32_t)score << 32;
    p |= (uint64_t)(depth & 63) << 26;
    p |= (uint64_t)(bound & 3) << 24;
    p |= (uint64_t)((move.first + 1) & 31) << 19;
    p |= (uint64_t)((move.second + 1) & 31) << 14;
    return p;
}

static inline void unpack_payload(uint64_t p, int& score, int& depth,
                                  uint8_t& bound, std::pair<int, int>& move) {
    score = (int)(uint32_t)(p >> 32);
    depth = (int)((p >> 26) & 63);
    bound = (uint8_t)((p >> 24) & 3);
    int r = (int)((p >> 19) & 31) - 1;
    int c = (int)((p >> 14) & 31) - 1;
    move = {r, c};
}

const TTEntry* tt_probe(uint64_t key) {
    if (!tt_enabled) return nullptr;
    Slot& s = slots[key & TT_MASK];
    // Snapshot the whole entry (key + payload) in one atomic load. The
    // snapshot is a value copy: no concurrent tt_store can change what the
    // caller later reads through tt_unpack.
    TTEntry snapshot = s.entry.load(std::memory_order_acquire);
    if (snapshot.key != key) return nullptr;
    return new (std::nothrow) TTEntry(snapshot);
}

void tt_unpack(const TTEntry* e, int& score, int& depth, uint8_t& bound,
               std::pair<int, int>& move) {
    unpack_payload(e->payload, score, depth, bound, move);
}

void tt_store(uint64_t key, int depth, int score, uint8_t bound, std::pair<int, int> move) {
    if (!tt_enabled) return;
    Slot& s = slots[key & TT_MASK];
    uint64_t payload = pack_payload(score, depth, bound, move);
    s.entry.store(TTEntry{key, payload}, std::memory_order_release);
}
