#pragma once

#include "file_manager.hpp"
#include "terminal.hpp"

namespace listless {

// Draws FileManager's column-major grid plus a single-line status line
// into terminal, using the geometry compute_grid() already computed for
// `fm`. Deliberately minimal: no per-column size/date fields, no F-key
// hint bar -- see docs/app-main-loop.md for what's narrowed here and
// why, the same posture viewer_render.cpp takes for the viewer.
void render_file_manager(const FileManager& fm, const Grid& grid, Terminal& terminal);

}  // namespace listless
