#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

namespace dxs {

// One-click Python queries that dump commonly-useful introspection info
// straight into the REPL output buffer. Saves the user from typing the same
// boilerplate (`import sys; print(sys.path)`) a hundred times.
class QuickActionsPanel : public IPanel {
public:
    std::string_view id()       const override { return "quick_actions"; }
    std::string_view category() const override { return L("sidebar.scripting"); }
    std::string_view title()    const override { return L("panel.quick_actions.title"); }
    void             draw()           override;
};

}  // namespace dxs
