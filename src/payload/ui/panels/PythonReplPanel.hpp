#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

#include <deque>
#include <string>

namespace dxs {

// Moved from ui/PythonPanel.{hpp,cpp} into the new framework. Same input
// history + scroll semantics, adapted to the IPanel contract so the ClickGui
// can host it inside the main content area rather than a second window.
class PythonReplPanel : public IPanel {
public:
    std::string_view id()       const override { return "python_repl"; }
    std::string_view category() const override { return L("sidebar.scripting"); }
    std::string_view title()    const override { return L("panel.python_repl.title"); }
    void             draw()           override;

    // Convenience — lets other panels (QuickActions) feed code through the same
    // output buffer so users see everything in one place.
    void submit_external(const std::string& src, bool echo_prompt);

private:
    void submit(const std::string& src);

    bool                     scroll_to_bottom_ = false;
    std::string              output_;
    char                     input_[4096]{};
    std::deque<std::string>  history_;
    int                      history_pos_      = -1;
};

}  // namespace dxs
