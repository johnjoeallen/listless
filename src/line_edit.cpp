#include "line_edit.hpp"

namespace listless {

LineEditStatus line_edit_key(std::string& text, KeyCode key) {
    if (key == '\r' || key == '\n') {
        return LineEditStatus::Submitted;
    }
    if (key == Key::Escape) {
        return LineEditStatus::Cancelled;
    }
    if (key == 8 || key == 127) {
        if (!text.empty()) {
            text.pop_back();
        }
        return LineEditStatus::Editing;
    }
    if (key >= 0x20 && key <= 0x7E) {
        text.push_back(static_cast<char>(key));
    }
    return LineEditStatus::Editing;
}

}  // namespace listless
