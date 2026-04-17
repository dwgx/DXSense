#pragma once

#include "ui/framework/IPanel.hpp"

namespace dxs {

// Placeholder for the full entity browser. Real enumeration needs the
// EntityManager handle, which we'll know after the Codex subsystem survey.
// Keeping the panel registered from day one so the ClickGui layout is stable
// across builds.
class EntitiesPanel : public IPanel {
public:
    std::string_view id()       const override { return "entities"; }
    std::string_view category() const override { return "Analysis"; }
    std::string_view title()    const override { return "Entities"; }
    void             draw()           override;
};

}  // namespace dxs
