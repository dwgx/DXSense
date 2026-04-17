#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

#include <string>
#include <vector>

namespace dxs {

// Live enumeration of Python-side game entities via the PythonBridge.
// Refreshing runs a tiny introspection script in-process that dumps a
// tab-separated table to the bridge's output sink; we parse it and render
// in a sortable ImGui table.
class EntitiesPanel : public IPanel {
public:
    std::string_view id()       const override { return "entities"; }
    std::string_view category() const override { return L("sidebar.analysis"); }
    std::string_view title()    const override { return L("panel.entities.title"); }
    void             draw()           override;

private:
    struct Row {
        std::string kind;     // Survivor / Hunter / Prop / ...
        std::string cls;      // Python class name
        std::string addr;     // PyObject* id
        std::string extras;   // freeform — hp, pos, flags, etc.
    };

    void kick_refresh();
    void absorb_output();

    std::vector<Row> rows_;
    double           last_kick_at_  = 0.0;
    bool             awaiting_      = false;
    char             filter_[128]   {};
    std::string      raw_buffer_;
};

}  // namespace dxs
