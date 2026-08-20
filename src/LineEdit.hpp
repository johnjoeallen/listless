#pragma once

#include <string>

#include "Key.hpp"

namespace listless {

enum class LineEditStatus { Editing, Submitted, Cancelled };

// Pure key-handling for a single-line modal text prompt (the original's
// LineEdit, os.hpp/oswidget.cpp) -- no rendering or input polling here,
// see App::run_prompt() for the interactive loop that drives this. See
// docs/app-main-loop.md for what's ported/narrowed relative to the
// original.
//
// Appends printable ASCII (0x20-0x7E) to `text`; Backspace (8 or 127,
// since terminals disagree on which byte a real keyboard's Backspace
// key sends) removes the last character; '\r'/'\n' returns Submitted
// without further modifying `text`; Key::Escape returns Cancelled;
// anything else leaves `text` unchanged and returns Editing.
LineEditStatus line_edit_key(std::string& text, KeyCode key);

}  // namespace listless
