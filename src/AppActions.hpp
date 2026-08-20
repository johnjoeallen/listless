#pragma once

#include "FileManager.hpp"
#include "Key.hpp"
#include "Viewer.hpp"

namespace listless {

// Pure key-dispatch for App's browsing (FileManager) screen: handles
// navigation/type-ahead directly against `fm`, returning an action only
// for the cases App itself must complete (open a file into a Viewer,
// quit). See docs/app-main-loop.md for the full key table and what's
// narrowed relative to the original's FileManager::Activate().
enum class BrowsingAction { None, OpenSelected, Quit };
BrowsingAction handle_browsing_key(FileManager& fm, const Grid& grid, KeyCode key);

// Pure key-dispatch for App's viewing (Viewer) screen: handles
// scrolling/mode-toggle/repeat-search/bookmarks directly against `v`,
// returning an action for the cases App itself must complete (run a
// modal prompt, close back to browsing). See docs/app-main-loop.md for
// the full key table and what's narrowed relative to the original's
// Viewer::handleKey(). There's no direct-quit action here (only
// Close) -- a Ctrl+<letter> "quit from the viewer" binding was tried
// and dropped: Terminal runs ncurses in cbreak() mode, which leaves
// XON/XOFF flow control active, so Ctrl+Q/Ctrl+S never reach the
// application, and other Ctrl+<letter> combinations risk the same
// fate. 'q' (Close) then 'q' again from browsing quits.
enum class ViewingAction {
    None,
    Close,
    PromptSearchForward,
    PromptSearchForwardCaseInsensitive,
    PromptGotoOffset,
    PromptGotoLine,
};
ViewingAction handle_viewing_key(Viewer& v, int visible_lines, int visible_width, KeyCode key);

}  // namespace listless
