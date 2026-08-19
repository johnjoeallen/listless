#pragma once

#include "terminal.hpp"
#include "viewer.hpp"

namespace listless {

// Draws viewer's visible page plus a single-line status line into
// terminal. Deliberately minimal: no syntax highlighting, no bold/
// underline "text with layout" rendering, a fixed tab width, and one
// status-line format -- see docs/07-file-viewer-core.md for what's
// narrowed here and why. Extending this (not rewriting it) is how
// subsystems 08/09 are expected to add real styling.
void render_viewer(const Viewer& viewer, Terminal& terminal);

}  // namespace listless
