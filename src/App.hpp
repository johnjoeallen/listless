#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "FileManager.hpp"
#include "Keyboard.hpp"
#include "Style.hpp"
#include "Terminal.hpp"
#include "Viewer.hpp"
#include "ViewerRender.hpp"

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
    // -> browse it; a regular file -> open it in the viewer. Closing a
    // directly opened viewer exits the program. Throws std::runtime_error
    // if `start_path` names a file that can't be opened (propagated from
    // Viewer's constructor).
    explicit App(std::filesystem::path start_path,
                 std::optional<std::string> syntax_style = std::nullopt,
                 std::optional<std::filesystem::path> syntax_dir = std::nullopt);

    // Constructs a viewer-only session over already-read stdin content
    // (`display_name` shown on the status line). Closing this directly
    // opened viewer exits the program.
    App(std::string stdin_content, std::string display_name,
        std::optional<std::string> syntax_style = std::nullopt,
        std::optional<std::filesystem::path> syntax_dir = std::nullopt);

    // Runs the interactive loop until the user quits. Returns a process
    // exit code (always 0 -- there is no failure path once the session
    // is running).
    int run();

  private:
    enum class Mode { Browsing, Viewing };

    // Loads styles_ from the package-installed system_styles_dir(), then
    // the user's default_styles_dir(). The personal directory has higher
    // precedence, so it can override or extend system styles by reusing a
    // style name (see docs/08-style-config.md).
    void load_styles();

    void run_browsing();
    void run_viewing();
    void open_selected();

    // Resolves and switches to the Style matching `viewer_`'s file
    // extension (the default style if none matches, or no path at all --
    // e.g. the stdin-viewer case), and resets highlight_cache_ since it's
    // indexed by the outgoing file's line numbering.
    void restyle_for_viewer();

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
    bool viewer_opened_from_browsing_ = false;
    bool quit_ = false;

    StyleSet styles_;
    Style* current_style_ = &styles_.default_style();
    std::optional<std::string> requested_style_;
    std::optional<std::filesystem::path> syntax_dir_;
    HighlightCache highlight_cache_;
};

}  // namespace listless
