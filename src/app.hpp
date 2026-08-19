#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "file_manager.hpp"
#include "keyboard.hpp"
#include "terminal.hpp"
#include "viewer.hpp"

namespace listless {

// The interactive session: owns the terminal/keyboard and alternates
// between browsing (FileManager) and viewing (Viewer) screens. This is
// deliberately not unit-tested -- it's glue over already-tested pieces
// (FileManager, Viewer, handle_browsing_key/handle_viewing_key,
// line_edit_key), coupled to a real terminal the same way the
// original's App::Run/FileManager::Activate were. See
// docs/app-main-loop.md.
class App {
  public:
    // `start_path` empty -> browse the current directory; a directory
    // -> browse it; a regular file -> open it in the viewer, with the
    // file's parent directory as the browsing screen "close viewer"
    // falls back to. Throws std::runtime_error if `start_path` names a
    // file that can't be opened (propagated from Viewer's constructor).
    explicit App(std::filesystem::path start_path);

    // Runs the interactive loop until the user quits. Returns a process
    // exit code (always 0 -- there is no failure path once the session
    // is running).
    int run();

  private:
    enum class Mode { Browsing, Viewing };

    void run_browsing();
    void run_viewing();
    void open_selected();

    // Drives line_edit_key() in a loop, rendering the prompt on the
    // terminal's last row. Returns the submitted text, or nullopt if
    // cancelled (Escape).
    std::optional<std::string> run_prompt(std::string_view prompt);

    void run_search_prompt(bool case_sensitive);
    void run_goto_offset_prompt();
    void run_goto_line_prompt();

    Terminal terminal_;
    Keyboard keyboard_;
    FileManager file_manager_;
    std::optional<Viewer> viewer_;
    Mode mode_ = Mode::Browsing;
    bool quit_ = false;
};

}  // namespace listless
