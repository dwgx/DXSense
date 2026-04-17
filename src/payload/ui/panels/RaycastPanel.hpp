#pragma once

#include "ui/framework/IPanel.hpp"

namespace dxs {

// Fires raycasts from the active camera and visualises results. Hooked up to
// the engine's own PhysX/physics ray call once we locate it (see subsystem
// survey output).
class RaycastPanel : public IPanel {
public:
    std::string_view id()       const override { return "raycast"; }
    std::string_view category() const override { return "Analysis"; }
    std::string_view title()    const override { return "Raycast"; }
    void             draw()           override;
};

}  // namespace dxs
