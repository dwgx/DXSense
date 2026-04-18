#pragma once

#include "IPanel.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace dxs {

// ═════════════════════════════════════════════════════════════════════════
//  TabbedPanel — host panel that groups multiple child IPanels under a
//  single sidebar entry. A segmented tab bar at the top switches between
//  children; the active child's draw() runs inside the content card.
//
//  Used to collapse the sidebar — the five dev-tool panels were cluttering
//  the nav rail when the user rarely needs more than one at a time. This
//  keeps the same surfaces available but groups them semantically.
//
//  Child panels don't need to know they're inside a tab; the host skips
//  the "page header" rendering that ClickGui does for standalone panels
//  so child titles don't compete with the tab bar for attention.
// ═════════════════════════════════════════════════════════════════════════
class TabbedPanel : public IPanel {
public:
    TabbedPanel(std::string id,
                std::string category,
                std::string title,
                std::string icon);

    // Append a child. Takes ownership. The panel's title() becomes its
    // tab label; its icon() renders in the tab (if non-empty).
    void add(std::unique_ptr<IPanel> child);

    std::string_view id()       const override { return id_; }
    std::string_view category() const override { return category_; }
    std::string_view title()    const override { return title_; }
    std::string_view icon()     const override { return icon_; }

    void draw() override;
    void on_first_show() override;

private:
    std::string id_, category_, title_, icon_;
    std::vector<std::unique_ptr<IPanel>> tabs_;
    int active_ = 0;
    bool tabs_first_shown_ = false;
};

}  // namespace dxs
