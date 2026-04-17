#pragma once

#include "ui/framework/IPanel.hpp"

namespace dxs {

// View / projection / bone matrices — the core of any ESP-style overlay.
// Populated after the subsystem survey locates the camera singleton.
class MatrixPanel : public IPanel {
public:
    std::string_view id()       const override { return "matrix"; }
    std::string_view category() const override { return "Analysis"; }
    std::string_view title()    const override { return "Matrix"; }
    void             draw()           override;
};

}  // namespace dxs
