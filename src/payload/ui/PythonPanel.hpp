#pragma once

#include <deque>
#include <string>

namespace dxs {

// ImGui panel that drives PythonBridge: a scrolling output console plus a
// single-line input with a command history. Kept in the ui/ folder alongside
// Overlay so the panel hierarchy stays flat.
class PythonPanel {
public:
    static PythonPanel& instance();

    void draw();                    // Called from Overlay::draw each frame.
    void set_visible(bool v) noexcept { visible_ = v; }
    bool visible() const noexcept   { return visible_; }

private:
    PythonPanel();

    void submit(const std::string& src);   // push to bridge + history.

    bool        visible_            = true;
    bool        scroll_to_bottom_   = false;
    std::string output_;                    // accumulated console text
    char        input_[4096]{};
    std::deque<std::string> history_;
    int         history_pos_        = -1;   // -1 means "current editing line"
};

}  // namespace dxs
