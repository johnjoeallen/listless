#pragma once

#include <vector>

#include "Style.hpp"
#include "SyntaxHighlight.hpp"
#include "Terminal.hpp"
#include "Viewer.hpp"

namespace listless {

// Caches per-line HighlightState exit-state (syntax_highlight.hpp) so
// render_viewer doesn't have to rescan a file from line 0 every frame
// just to derive the entry state for the top of the visible window.
// Grows incrementally, in order, as further-down lines are visited --
// jumping back to an already-visited line is O(1); jumping forward past
// the cached range costs one linear catch-up scan, then is O(1) again.
// reset() must be called whenever the underlying line indexing changes:
// a new file is opened, or word wrap is toggled.
class HighlightCache {
  public:
    void reset();

    // The HighlightState resulting from lines [0, line) of `viewer`,
    // computed against `style`, extending the cache as needed.
    HighlightState state_before(const Viewer& viewer, const Style& style, int line);

  private:
    std::vector<HighlightState> end_states_;  // end_states_[i] == state after line i
};

// Draws viewer's visible page plus a single-line status line into
// terminal -- dispatching on viewer.display_mode() to either the text
// renderer or the hex-dump renderer (subsystem 10). This overload draws
// text-mode lines uncoloured (a single default-coloured span per line),
// for callers with no Style to render against.
void render_viewer(const Viewer& viewer, Terminal& terminal);

// As above, but syntax-highlights text-mode lines against `style` (see
// syntax_highlight.hpp), using and extending `highlight_cache` to avoid
// rescanning from line 0 on every call. Selection/search highlighting
// composes with syntax colours by taking over foreground/background
// (dropping bold/underline) only within the selected byte range; syntax
// colours still show through the rest of the line.
void render_viewer(const Viewer& viewer, Terminal& terminal, const Style& style,
                   HighlightCache& highlight_cache);

}  // namespace listless
