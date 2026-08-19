#include "color_pair_table.hpp"

namespace listless {

ColorPairTable::ColorPairTable(int max_pairs, AllocatedCallback on_allocated)
    : max_pairs_(max_pairs), on_allocated_(std::move(on_allocated)) {}

int ColorPairTable::pair_for(Color fg, Color bg) {
    auto key = std::make_pair(fg, bg);

    auto it = pairs_.find(key);
    if (it != pairs_.end()) {
        return it->second;
    }

    if (next_ >= max_pairs_) {
        return 0;  // exhausted: fall back to the default pair
    }

    int id = next_++;
    pairs_[key] = id;

    if (on_allocated_) {
        on_allocated_(id, fg, bg);
    }

    return id;
}

}  // namespace listless
