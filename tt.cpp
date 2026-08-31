#include "tt.hpp"
#include <vector>

static std::vector<TTEntry> table(TT_SIZE);
static bool tt_enabled = true;

void tt_clear() {
    table.assign(TT_SIZE, TTEntry{});
}

void tt_set_enabled(bool enabled) {
    tt_enabled = enabled;
}

const TTEntry* tt_probe(uint64_t key) {
    if (!tt_enabled) return nullptr;
    TTEntry& e = table[key & TT_MASK];
    if (e.depth >= 0 && e.key == key) return &e;
    return nullptr;
}

void tt_store(uint64_t key, int depth, int score, uint8_t bound, std::pair<int, int> move) {
    if (!tt_enabled) return;
    TTEntry& e = table[key & TT_MASK];
    e.key = key;
    e.score = score;
    e.depth = (int16_t)depth;
    e.bound = bound;
    e.move_row = (int8_t)move.first;
    e.move_col = (int8_t)move.second;
}
