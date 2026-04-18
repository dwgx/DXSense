#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

// View / projection matrix inspector. Pure consumer — the data comes from
// the shared CameraSampler, which runs one throttled Python call per frame
// and caches the matrices. This panel has no probe of its own.
class MatrixPanel : public IPanel {
public:
    std::string_view id()       const override { return "matrix"; }
    std::string_view category() const override { return L("sidebar.inspection"); }
    std::string_view title()    const override { return L("panel.matrix.title"); }
    std::string_view icon()     const override { return ICON_MATRIX; }
    void             draw()           override;
};

}  // namespace dxs
