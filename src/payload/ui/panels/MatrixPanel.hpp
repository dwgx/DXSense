#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

namespace dxs {

// View / projection matrix inspector. Two data sources, whichever returns
// first wins:
//   1. Python probe   — attempts a few common NeoX3 accessors via Python
//      (e.g. `BigWorld.camera().view_matrix`) and shows whatever comes back.
//   2. DX11 probe     — snoops the active vertex-shader constant buffer at
//      slot 0 during Present, extracts a plausible row-major 4x4.
class MatrixPanel : public IPanel {
public:
    std::string_view id()       const override { return "matrix"; }
    std::string_view category() const override { return L("sidebar.analysis"); }
    std::string_view title()    const override { return L("panel.matrix.title"); }
    void             draw()           override;

private:
    void draw_matrix_box(const char* title, const float m[16], bool have);
    void kick_python_probe();

    float view_[16]{};
    float proj_[16]{};
    bool  have_view_ = false;
    bool  have_proj_ = false;
    double last_kick_at_ = 0.0;
};

}  // namespace dxs
