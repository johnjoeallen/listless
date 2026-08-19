#include "app_actions.hpp"

#include <cctype>

namespace listless {

BrowsingAction handle_browsing_key(FileManager& fm, const Grid& grid, KeyCode key) {
    switch (key) {
        case Key::Up:
            fm.move_up(grid);
            return BrowsingAction::None;
        case Key::Down:
            fm.move_down(grid);
            return BrowsingAction::None;
        case Key::Left:
            fm.move_left(grid);
            return BrowsingAction::None;
        case Key::Right:
            fm.move_right(grid);
            return BrowsingAction::None;
        case Key::Home:
            fm.move_home(grid);
            return BrowsingAction::None;
        case Key::End:
            fm.move_end(grid);
            return BrowsingAction::None;
        case Key::PageUp:
            fm.move_page_up(grid);
            return BrowsingAction::None;
        case Key::PageDown:
            fm.move_page_down(grid);
            return BrowsingAction::None;

        case '\r':
        case '\n':
            if (fm.size() > 0 && fm.selected().is_directory) {
                fm.enter_selected(grid);
                return BrowsingAction::None;
            }
            return fm.size() > 0 ? BrowsingAction::OpenSelected : BrowsingAction::None;

        case 8:
        case 127:
            fm.type_ahead_backspace(/*directories=*/false, grid);
            return BrowsingAction::None;

        case 'q':
        case 'Q':
        case Key::Escape:
            return BrowsingAction::Quit;

        default:
            if (key >= 0x20 && key <= 0x7E) {
                fm.type_ahead_append(static_cast<char>(key), /*directories=*/false, grid);
            }
            return BrowsingAction::None;
    }
}

namespace {

bool is_bookmark_jump_key(KeyCode key, int& slot) {
    if (key >= '0' && key <= '9') {
        slot = key - '0';
        return true;
    }
    return false;
}

bool is_bookmark_set_key(KeyCode key, int& slot) {
    for (int digit = 0; digit <= 9; ++digit) {
        if (key == alt_key(static_cast<char>('0' + digit))) {
            slot = digit;
            return true;
        }
    }
    return false;
}

}  // namespace

ViewingAction handle_viewing_key(Viewer& v, int visible_lines, int visible_width, KeyCode key) {
    bool hex = v.display_mode() == DisplayMode::Hex;

    switch (key) {
        case Key::Up:
            if (hex) {
                v.hex_scroll_line_up();
            } else {
                v.scroll_line_up();
            }
            return ViewingAction::None;
        case Key::Down:
            if (hex) {
                v.hex_scroll_line_down(visible_lines);
            } else {
                v.scroll_line_down(visible_lines);
            }
            return ViewingAction::None;
        case Key::PageUp:
            if (hex) {
                v.hex_scroll_page_up(visible_lines);
            } else {
                v.scroll_page_up(visible_lines);
            }
            return ViewingAction::None;
        case Key::PageDown:
            if (hex) {
                v.hex_scroll_page_down(visible_lines);
            } else {
                v.scroll_page_down(visible_lines);
            }
            return ViewingAction::None;
        case Key::Home:
            if (hex) {
                v.hex_scroll_to_top();
            } else {
                v.scroll_to_top();
            }
            return ViewingAction::None;
        case Key::End:
            if (hex) {
                v.hex_scroll_to_bottom(visible_lines);
            } else {
                v.scroll_to_bottom(visible_lines);
            }
            return ViewingAction::None;
        case Key::Left:
            if (!hex) {
                v.scroll_left();
            }
            return ViewingAction::None;
        case Key::Right:
            if (!hex) {
                v.scroll_right(visible_width);
            }
            return ViewingAction::None;

        case 'h':
        case 'H':
            if (hex) {
                v.switch_to_text_mode();
            } else {
                v.switch_to_hex_mode();
            }
            return ViewingAction::None;

        case 's':
        case 'S':
        case '/':
            return ViewingAction::PromptSearchForward;
        case 'f':
        case 'F':
            return ViewingAction::PromptSearchForwardCaseInsensitive;
        case 'a':
            v.repeat_search(/*forward=*/true);
            return ViewingAction::None;
        case 'A':
            v.repeat_search(/*forward=*/false);
            return ViewingAction::None;

        case 'g':
        case 'G':
            if (hex) {
                return ViewingAction::PromptGotoOffset;
            }
            return ViewingAction::None;

        case ':':
            if (!hex) {
                return ViewingAction::PromptGotoLine;
            }
            return ViewingAction::None;

        case 'q':
        case 'Q':
        case Key::Escape:
            return ViewingAction::Close;

        default: {
            int slot = 0;
            if (is_bookmark_set_key(key, slot)) {
                v.set_bookmark(slot);
                return ViewingAction::None;
            }
            if (is_bookmark_jump_key(key, slot)) {
                v.jump_to_bookmark(slot);
                return ViewingAction::None;
            }
            return ViewingAction::None;
        }
    }
}

}  // namespace listless
