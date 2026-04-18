#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/MdiIcons.hpp"

#include <array>

namespace dxs {

class OverviewPanel : public IPanel {
public:
    std::string_view id()       const override { return "overview"; }
    std::string_view category() const override { return L("sidebar.core"); }
    std::string_view title()    const override { return L("panel.overview.title"); }
    std::string_view icon()     const override { return MDI_VIEW_DASHBOARD_OUTLINE; }
    void             draw()           override;

private:
    void push_metric_samples(float fps, float frame_ms, int frame_id);

    std::array<float, 20> fps_history_{};
    std::array<float, 20> frame_history_{};
    int                   history_count_   = 0;
    int                   history_head_    = 0;
    int                   last_frame_seen_ = -1;
};

}  // namespace dxs
