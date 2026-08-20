#include "App.hpp"

#include <algorithm>
#include <cstdlib>

#include "AppActions.hpp"
#include "FileManagerRender.hpp"
#include "LineEdit.hpp"
#include "ViewerRender.hpp"

namespace listless {

App::App(std::filesystem::path start_path)
    : keyboard_(terminal_), file_manager_(std::filesystem::current_path()) {
    load_styles();

    if (start_path.empty()) {
        return;
    }

    if (std::filesystem::is_directory(start_path)) {
        file_manager_.change_directory(start_path);
        return;
    }

    std::filesystem::path parent =
        start_path.has_parent_path() ? start_path.parent_path() : std::filesystem::current_path();
    file_manager_.change_directory(parent);
    viewer_.emplace(start_path);
    mode_ = Mode::Viewing;
    restyle_for_viewer();
}

App::App(std::string stdin_content, std::string display_name)
    : terminal_(/*read_from_tty=*/true),
      keyboard_(terminal_),
      file_manager_(std::filesystem::current_path()) {
    load_styles();

    viewer_.emplace(std::move(stdin_content), std::move(display_name));
    mode_ = Mode::Viewing;
    restyle_for_viewer();
}

void App::load_styles() {
    load_config_dir(styles_, system_styles_dir());
    load_config(styles_, default_config_path());
    load_config_dir(styles_, default_styles_dir());
}

int App::run() {
    while (!quit_) {
        if (mode_ == Mode::Browsing) {
            run_browsing();
        } else {
            run_viewing();
        }
    }
    return 0;
}

void App::run_browsing() {
    while (!quit_ && mode_ == Mode::Browsing) {
        Grid grid = compute_grid(terminal_.width(), terminal_.height());
        render_file_manager(file_manager_, grid, terminal_);
        terminal_.refresh();

        KeyCode key = keyboard_.read_key();
        if (key == Key::Resize) {
            continue;
        }

        switch (handle_browsing_key(file_manager_, grid, key)) {
            case BrowsingAction::OpenSelected:
                open_selected();
                break;
            case BrowsingAction::Quit:
                quit_ = true;
                break;
            case BrowsingAction::None:
                break;
        }
    }
}

void App::restyle_for_viewer() {
    current_style_ = &styles_.default_style();
    if (viewer_ && viewer_->path().has_extension()) {
        if (Style* match = styles_.style_for_extension(viewer_->path().extension().string())) {
            current_style_ = match;
        }
    }
    highlight_cache_.reset();
}

void App::open_selected() {
    try {
        viewer_.emplace(file_manager_.selected().path);
        mode_ = Mode::Viewing;
        restyle_for_viewer();
    } catch (const std::exception& e) {
        std::string msg = std::string("Error: ") + e.what();
        terminal_.clear_to_eol(0, 0, Color::White, Color::Black);
        terminal_.put_text(0, 0, msg, Color::White, Color::Black);
        terminal_.refresh();
        keyboard_.read_key();
    }
}

void App::run_viewing() {
    while (!quit_ && mode_ == Mode::Viewing) {
        int visible_lines = std::max(0, terminal_.height() - 1);
        int visible_width = terminal_.width();

        render_viewer(*viewer_, terminal_, *current_style_, highlight_cache_);
        terminal_.refresh();

        KeyCode key = keyboard_.read_key();
        if (key == Key::Resize) {
            continue;
        }

        switch (handle_viewing_key(*viewer_, visible_lines, visible_width, key)) {
            case ViewingAction::Close:
                viewer_.reset();
                mode_ = Mode::Browsing;
                break;
            case ViewingAction::PromptSearchForward:
                run_search_prompt(/*case_sensitive=*/true);
                break;
            case ViewingAction::PromptSearchForwardCaseInsensitive:
                run_search_prompt(/*case_sensitive=*/false);
                break;
            case ViewingAction::PromptGotoOffset:
                run_goto_offset_prompt();
                break;
            case ViewingAction::PromptGotoLine:
                run_goto_line_prompt();
                break;
            case ViewingAction::None:
                break;
        }
    }
}

std::optional<std::string> App::run_prompt(std::string_view prompt) {
    std::string text;
    int row = std::max(0, terminal_.height() - 1);

    while (true) {
        std::string display = std::string(prompt) + ": " + text;
        int width = terminal_.width();
        if (static_cast<int>(display.size()) > width) {
            display = display.substr(display.size() - static_cast<std::size_t>(width));
        }

        terminal_.clear_to_eol(0, row, Color::LightGray, Color::Black);
        terminal_.put_text(0, row, display, Color::LightGray, Color::Black);
        terminal_.refresh();

        KeyCode key = keyboard_.read_key();
        if (key == Key::Resize) {
            continue;
        }

        switch (line_edit_key(text, key)) {
            case LineEditStatus::Submitted:
                return text;
            case LineEditStatus::Cancelled:
                return std::nullopt;
            case LineEditStatus::Editing:
                break;
        }
    }
}

void App::run_search_prompt(bool case_sensitive) {
    std::optional<std::string> pattern = run_prompt("Search");
    if (!pattern || pattern->empty()) {
        return;
    }

    if (viewer_->search_forward(*pattern, case_sensitive)) {
        viewer_->ensure_selection_visible(std::max(0, terminal_.height() - 1), terminal_.width());
    }
}

void App::run_goto_offset_prompt() {
    std::optional<std::string> text = run_prompt("Offset (hex)");
    if (!text || text->empty()) {
        return;
    }

    char* end = nullptr;
    unsigned long offset = std::strtoul(text->c_str(), &end, 16);
    if (end != text->c_str()) {
        viewer_->hex_goto_offset(static_cast<std::size_t>(offset));
    }
}

void App::run_goto_line_prompt() {
    std::optional<std::string> text = run_prompt("Line");
    if (!text || text->empty()) {
        return;
    }

    char* end = nullptr;
    long line = std::strtol(text->c_str(), &end, 10);
    if (end != text->c_str() && line > 0) {
        viewer_->goto_line(static_cast<int>(line - 1));
    }
}

}  // namespace listless
