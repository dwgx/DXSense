#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/MdiIcons.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace dxs {

struct Interaction {
    double        ts = 0.0;
    std::string   source;
    std::string   name;
    std::uint64_t actor_uid = 0;
    std::uint64_t target_uid = 0;
    std::string   args_json;
    std::string   raw;
};

class InteractionFatherPanel : public IPanel {
public:
    std::string_view id()       const override { return "interaction_father"; }
    std::string_view category() const override { return L("sidebar.lab"); }
    std::string_view title()    const override { return L("panel.interaction_father.title"); }
    std::string_view icon()     const override { return MDI_GESTURE_TAP; }
    void             draw()           override;

    void append(Interaction e);

    static InteractionFatherPanel* instance();

private:
    void install_hook_helper();
    void drain_python_hooks();

    static constexpr std::size_t k_capacity = 1000;

    std::mutex                        mtx_;
    std::array<Interaction, k_capacity> ring_{};
    std::size_t                       head_ = 0;
    std::size_t                       count_ = 0;

    bool                              hook_installed_ = false;
    bool                              paused_ = false;
    char                              filter_name_[128]{};
    char                              filter_source_[32]{"all"};
    int                               selected_idx_ = -1;
};

}  // namespace dxs
