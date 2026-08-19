#pragma once

#include <memory>
#include <string_view>

#include "color.hpp"

namespace listless {

// A screen-buffer terminal, RAII-owning the terminal session for its
// lifetime (constructing enters the alternate screen / raw mode;
// destructing restores the terminal). One implementation file per
// platform (platform/<os>/terminal.cpp) defines Impl and every method
// body -- this header has no platform-specific code in it.
//
// Coordinates are 0-indexed, unlike the original's DOS/conio 1-indexed
// convention -- there are no legacy call sites to preserve here.
class Terminal {
  public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(Terminal&&) = delete;

    int width() const;
    int height() const;

    void move_cursor(int x, int y);
    void put_text(int x, int y, std::string_view text, Color fg, Color bg);
    void clear_to_eol(int x, int y, Color fg, Color bg);
    void clear();

    // Scrolls the full-width region of rows [top, bottom) (bottom
    // exclusive) up by `lines` rows (negative scrolls down), shifting
    // already-drawn content rather than requiring a redraw.
    void scroll_region(int top, int bottom, int lines);

    void refresh();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace listless
