#include "tt.hpp"
#include <vector>

// SMP safety (Lazy SMP), final design: two atomic words per slot.
//   key     — the position's hash; 0 means "no valid entry"
//   payload — packed score/depth/bound/move, written atomically as one unit
//
// Writer: invalidate key (0) -> store payload -> publish key (release).
// Reader: acquire key -> if it matches, load payload.
//
// A reader therefore sees either the complete old entry, the complete new
// entry, or a miss (key==0 or different key) — never a torn mix. The
// writer-vs-writer payload race (two workers storing to the same slot) is
// inherent to always-replace and harmless: the loser's payload is simply
// overwritten, and any reader that already validated the key may read either
// complete payload, both of which are legitimate entries for that position.
struct Slot {
    std::atomic<uint64_t> key{0};
    std::atomic<uint64_t> payload{0};
};
static std::vector<Slot> slots(TT_SIZE);
static bool tt_enabled = true;

void tt_clear() {
    // Single-threaded context (program startup, between searches).
    for (auto& s : slots) {
        s.key.store(0, std::memory_order_relaxed);
        s.payload.store(0, std::memory_order_relaxed);
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
    uint64_t k = s.key.load(std::memory_order_acquire);
    if (k != key) return nullptr;
    // Hand the caller an opaque pointer to the slot; payload is read from it
    // atomically inside tt_unpack. (TTEntry is the public-facing handle type;
    // its single atomic<uint64_t> word member is unused by this impl.)
    return reinterpret_cast<const TTEntry*>(&s);
}

void tt_unpack(const TTEntry* e, int& score, int& depth, uint8_t& bound,
               std::pair<int, int>& move) {
    const Slot* s = reinterpret_cast<const Slot*>(e);
    uint64_t p = s->payload.load(std::memory_order_relaxed);
    unpack_payload(p, score, depth, bound, move);
}

void tt_store(uint64_t key, int depth, int score, uint8_t bound, std::pair<int, int> move) {
    if (!tt_enabled) return;
    Slot& s = slots[key & TT_MASK];
    uint64_t payload = pack_payload(score, depth, bound, move);
    s.key.store(0, std::memory_order_relaxed);
    s.payload.store(payload, std::memory_order_relaxed);
    s.key.store(key, std::memory_order_release);
}
