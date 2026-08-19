#pragma once

#include <functional>
#include <map>
#include <utility>

#include "color.hpp"

namespace listless {

// Lazily allocates small integer "pair" ids for (foreground, background)
// colour combinations -- the model ncurses (and similar cell-attribute
// terminal APIs) use instead of independent fg/bg attributes. Id 0 is
// reserved as the default pair and is returned once `max_pairs` distinct
// combinations have already been allocated, rather than failing.
class ColorPairTable {
  public:
    // `on_allocated`, if given, is invoked exactly once per newly
    // allocated pair id, with its (fg, bg) -- the hook a real backend
    // uses to register the pair with the terminal (e.g. ncurses'
    // init_pair()).
    using AllocatedCallback = std::function<void(int pair_id, Color fg, Color bg)>;

    explicit ColorPairTable(int max_pairs, AllocatedCallback on_allocated = {});

    // Returns the pair id for (fg, bg), allocating one on first request.
    int pair_for(Color fg, Color bg);

  private:
    int max_pairs_;
    AllocatedCallback on_allocated_;
    std::map<std::pair<Color, Color>, int> pairs_;
    int next_ = 1;
};

}  // namespace listless
