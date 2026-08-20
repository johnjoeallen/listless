#include "ViewerRender.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>

namespace listless {

namespace {

constexpr int kTabWidth = 8;  // placeholder until subsystem 08 wires in Style::iTabWidth
constexpr Color kNormalFg = Color::LightGray;
constexpr Color kNormalBg = Color::Black;
constexpr Color kSelectedFg = Color::Black;
constexpr Color kSelectedBg = Color::LightGray;

struct ExpandedLine {
    std::string text;
    // raw_to_expanded[i] is the expanded-text column that raw byte index
    // i (0..raw.size(), inclusive) maps to -- lets any raw byte offset
    // (a selection bound, a ColorSpan bound) be translated to where it
    // lands once tabs are expanded.
    std::vector<std::size_t> raw_to_expanded;
};

ExpandedLine expand_line(std::string_view raw) {
    ExpandedLine result;
    result.text.reserve(raw.size());
    result.raw_to_expanded.reserve(raw.size() + 1);
    result.raw_to_expanded.push_back(0);

    for (char c : raw) {
        if (c == '\t') {
            std::size_t next_stop = ((result.text.size() / kTabWidth) + 1) * kTabWidth;
            result.text.append(next_stop - result.text.size(), ' ');
        } else {
            result.text.push_back(c);
        }
        result.raw_to_expanded.push_back(result.text.size());
    }

    return result;
}

// Paints text[seg_start, seg_end) with one attribute, clipped to the
// visible [column, column+width) window.
void paint_run(Terminal& terminal, int row, int column, int width, std::string_view text,
               std::size_t seg_start, std::size_t seg_end, Color fg, Color bg, bool bold,
               bool underlined) {
    std::size_t win_start = std::max(seg_start, static_cast<std::size_t>(column));
    std::size_t win_end =
        std::min(seg_end, static_cast<std::size_t>(column) + static_cast<std::size_t>(width));
    if (win_start >= win_end) return;

    terminal.put_text(static_cast<int>(win_start - static_cast<std::size_t>(column)), row,
                      text.substr(win_start, win_end - win_start), fg, bg, bold, underlined);
}

// Paints one syntax-coloured span [exp_start, exp_end), splitting out
// the portion (if any) that overlaps [sel_start, sel_end) to draw in
// reverse video instead -- selection wins within its range, syntax
// colour shows through everywhere else on the line.
void paint_span(Terminal& terminal, int row, int column, int width, std::string_view text,
                std::size_t exp_start, std::size_t exp_end, Color color, bool bold, bool underlined,
                bool has_selection, std::size_t sel_start, std::size_t sel_end) {
    if (has_selection && sel_start < sel_end) {
        std::size_t inter_start = std::max(exp_start, sel_start);
        std::size_t inter_end = std::min(exp_end, sel_end);
        if (inter_start < inter_end) {
            if (exp_start < inter_start) {
                paint_run(terminal, row, column, width, text, exp_start, inter_start, color,
                          kNormalBg, bold, underlined);
            }
            paint_run(terminal, row, column, width, text, inter_start, inter_end, kSelectedFg,
                      kSelectedBg, /*bold=*/false, /*underlined=*/false);
            if (inter_end < exp_end) {
                paint_run(terminal, row, column, width, text, inter_end, exp_end, color, kNormalBg,
                          bold, underlined);
            }
            return;
        }
    }

    paint_run(terminal, row, column, width, text, exp_start, exp_end, color, kNormalBg, bold,
              underlined);
}

void draw_text_line(Terminal& terminal, int row, const Viewer& viewer, int line_index, int column,
                    int width, const std::vector<ColorSpan>& spans) {
    std::string_view raw = viewer.line_text(line_index);
    ExpandedLine expanded = expand_line(raw);

    bool has_selection = viewer.selection().line == line_index;
    std::size_t sel_start = 0;
    std::size_t sel_end = 0;
    if (has_selection) {
        const Selection& sel = viewer.selection();
        std::size_t raw_start = std::min(sel.pos, raw.size());
        std::size_t raw_end =
            sel.count == kWholeLine ? raw.size() : std::min(sel.pos + sel.count, raw.size());
        sel_start = expanded.raw_to_expanded[raw_start];
        sel_end = expanded.raw_to_expanded[raw_end];
    }

    terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);

    for (const ColorSpan& span : spans) {
        std::size_t raw_start = std::min(span.offset, raw.size());
        std::size_t raw_end = std::min(span.offset + span.length, raw.size());
        if (raw_start >= raw_end) continue;

        std::size_t exp_start = expanded.raw_to_expanded[raw_start];
        std::size_t exp_end = expanded.raw_to_expanded[raw_end];
        paint_span(terminal, row, column, width, expanded.text, exp_start, exp_end, span.color,
                   span.bold, span.underlined, has_selection, sel_start, sel_end);
    }
}

// "OOOOOOOO  HH HH HH HH  HH HH HH HH  HH HH HH HH  HH HH HH HH |ASCII...........|"
// -- a standard hex-dump layout, not a reproduction of the original's
// corrupted-encoding separator glyphs (see docs/10-hex-mode-viewer.md).
// Non-printable bytes (outside 0x20-0x7E) show as '.' in the gutter.
std::string format_hex_line(std::string_view bytes, int line_index) {
    std::string out;
    char offset_field[32];
    char byte_field[8];

    std::snprintf(offset_field, sizeof(offset_field), "%08zX",
                  static_cast<std::size_t>(line_index) * Viewer::kBytesPerHexLine);
    out += offset_field;
    out += "  ";

    for (int i = 0; i < Viewer::kBytesPerHexLine; ++i) {
        if (i != 0 && i % 4 == 0) {
            out += ' ';
        }
        if (i < static_cast<int>(bytes.size())) {
            std::snprintf(byte_field, sizeof(byte_field), "%02X ",
                          static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]));
            out += byte_field;
        } else {
            out += "   ";
        }
    }

    out += '|';
    for (int i = 0; i < Viewer::kBytesPerHexLine; ++i) {
        if (i < static_cast<int>(bytes.size())) {
            auto c = static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);
            out += (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.';
        } else {
            out += ' ';
        }
    }
    out += '|';

    return out;
}

void draw_hex_line(Terminal& terminal, int row, const Viewer& viewer, int line_index, int width) {
    std::string line = format_hex_line(viewer.hex_line_bytes(line_index), line_index);
    if (static_cast<int>(line.size()) > width) {
        line.resize(static_cast<std::size_t>(width));
    }

    terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);
    if (!line.empty()) {
        terminal.put_text(0, row, line, kNormalFg, kNormalBg);
    }
}

void draw_status_line(Terminal& terminal, const Viewer& viewer, int width) {
    int total = viewer.line_count();
    int current = viewer.top_line() + 1;
    int percent = total > 0 ? std::min(100, (current * 100) / total) : 100;

    std::string status = viewer.display_name() + " | line " + std::to_string(current) + "/" +
                         std::to_string(total) + " (" + std::to_string(percent) + "%) | col " +
                         std::to_string(viewer.column() + 1);

    if (static_cast<int>(status.size()) > width) {
        status.resize(static_cast<std::size_t>(width));
    }

    terminal.clear_to_eol(0, 0, kSelectedFg, kSelectedBg);
    terminal.put_text(0, 0, status, kSelectedFg, kSelectedBg);
}

void render_text_mode(const Viewer& viewer, Terminal& terminal, int width, int height,
                      const Style& style, HighlightCache& highlight_cache) {
    HighlightState state = highlight_cache.state_before(viewer, style, viewer.top_line());

    for (int row = 1; row < height; ++row) {
        int line_index = viewer.top_line() + (row - 1);
        if (line_index < viewer.line_count()) {
            std::vector<ColorSpan> spans =
                highlight_line(viewer.line_text(line_index), style, state);
            draw_text_line(terminal, row, viewer, line_index, viewer.column(), width, spans);
        } else {
            terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);
        }
    }
}

void render_hex_mode(const Viewer& viewer, Terminal& terminal, int width, int height) {
    for (int row = 1; row < height; ++row) {
        int line_index = viewer.hex_top_line() + (row - 1);
        if (line_index < viewer.hex_line_count()) {
            draw_hex_line(terminal, row, viewer, line_index, width);
        } else {
            terminal.clear_to_eol(0, row, kNormalFg, kNormalBg);
        }
    }
}

}  // namespace

void HighlightCache::reset() { end_states_.clear(); }

HighlightState HighlightCache::state_before(const Viewer& viewer, const Style& style, int line) {
    if (line <= 0) return HighlightState{};

    std::size_t target = static_cast<std::size_t>(line) - 1;
    if (target < end_states_.size()) return end_states_[target];

    HighlightState state = end_states_.empty() ? HighlightState{} : end_states_.back();
    for (std::size_t i = end_states_.size(); i <= target; ++i) {
        highlight_line(viewer.line_text(static_cast<int>(i)), style, state);
        end_states_.push_back(state);
    }
    return end_states_[target];
}

void render_viewer(const Viewer& viewer, Terminal& terminal) {
    static const Style kUnstyled("Default");
    HighlightCache throwaway_cache;
    render_viewer(viewer, terminal, kUnstyled, throwaway_cache);
}

void render_viewer(const Viewer& viewer, Terminal& terminal, const Style& style,
                   HighlightCache& highlight_cache) {
    int width = terminal.width();
    int height = terminal.height();

    draw_status_line(terminal, viewer, width);

    if (viewer.display_mode() == DisplayMode::Hex) {
        render_hex_mode(viewer, terminal, width, height);
        return;
    }

    render_text_mode(viewer, terminal, width, height, style, highlight_cache);
}

}  // namespace listless
