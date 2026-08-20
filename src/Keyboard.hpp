#pragma once

#include <memory>

#include "Key.hpp"
#include "Terminal.hpp"

namespace listless {

// Keyboard input, translated into Listless's KeyCode space (see
// key.hpp). One implementation file per platform
// (platform/<os>/keyboard.cpp) defines Impl and every method body.
//
// Takes a Terminal& purely to enforce -- at the type level, and by
// requiring one to already exist -- that the terminal session (and so
// the underlying input backend) is initialized before keyboard input is
// read.
class Keyboard {
  public:
    explicit Keyboard(Terminal&);
    ~Keyboard();

    Keyboard(const Keyboard&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;
    Keyboard(Keyboard&&) = delete;
    Keyboard& operator=(Keyboard&&) = delete;

    // Blocking read of the next key.
    KeyCode read_key();

    // Non-blocking check for a pending key (kbhit()-equivalent).
    bool key_available();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace listless
