#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace dxs {

class AcObservatoryPanel : public IPanel {
public:
    std::string_view id()       const override { return "ac_observatory"; }
    std::string_view category() const override { return L("sidebar.inspection"); }
    std::string_view title()    const override { return L("panel.ac_observatory.title"); }
    std::string_view icon()     const override { return ICON_SEARCH; }
    void             draw()           override;

    struct Entry {
        std::chrono::steady_clock::time_point ts{};
        std::string                           dll;
        std::string                           func;
        std::string                           args_summary;
        std::vector<uint8_t>                  body_sample;
        uint32_t                              tid = 0;
    };

    void append(Entry e);

    bool arm_hooks();
    void disarm_hooks();
    bool armed() const { return armed_.load(); }

    static AcObservatoryPanel* instance();

private:
    static constexpr size_t k_capacity = 512;

    std::mutex                    mtx_;
    std::array<Entry, k_capacity> ring_{};
    size_t                        head_ = 0;
    size_t                        count_ = 0;
    std::atomic_bool              armed_{false};
    std::atomic_bool              attempt_made_{false};

    bool paused_ = false;
    char filter_func_[128]{};
    char filter_dll_[64]{"all"};
    int  selected_idx_ = -1;
};

}  // namespace dxs
