#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/MdiIcons.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace dxs {

// Live enumeration of Python-side game entities via the PythonBridge.
// Refreshing runs a tiny introspection script in-process that dumps a
// tab-separated table to the bridge's output sink; we parse it and render
// in a sortable ImGui table.
class EntitiesPanel : public IPanel {
public:
    std::string_view id()       const override { return "entities"; }
    std::string_view category() const override { return L("sidebar.inspection"); }
    std::string_view title()    const override { return L("panel.entities.title"); }
    std::string_view icon()     const override { return MDI_ACCOUNT_GROUP_OUTLINE; }
    void             draw()           override;
    void             on_first_show() override;

    struct Row {
        std::string kind;     // Survivor / Hunter / Prop / ...
        std::string cls;      // Python class name
        std::string addr;     // PyObject* id
        std::string extras;   // freeform — hp, pos, flags, etc.
    };

    // Exposed so HUD widgets (Radar) and other panels can read live data.
    const std::vector<Row>& rows() const { return rows_; }
    static EntitiesPanel*   global();

private:
    void kick_refresh();
    void absorb_output();
    bool category_enabled(std::string_view kind) const;

    std::vector<Row>               rows_;
    double                         last_kick_at_    = 0.0;
    double                         last_auto_refresh_ = 0.0;
    bool                           awaiting_        = false;
    bool                           auto_refresh_    = true;
    char                           filter_[128]    {};
    std::string                    raw_buffer_;
    std::unordered_set<std::string> cat_hide_;   // categories toggled OFF
};

}  // namespace dxs
