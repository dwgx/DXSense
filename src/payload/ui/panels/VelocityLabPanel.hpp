#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace dxs {

class VelocityLabPanel : public IPanel {
public:
    std::string_view id()       const override { return "velocity_lab"; }
    std::string_view category() const override { return L("sidebar.modules"); }
    std::string_view title()    const override { return L("panel.velocity_lab.title"); }
    std::string_view icon()     const override { return ICON_RADAR; }
    void             draw()           override;

private:
    static constexpr std::size_t kRingSize = 300;  // ~=10 s @ 30 Hz

    struct Sample {
        double t = 0.0;
        float  speed_m_s = 0.0f;
        float  x = 0.0f;
        float  y = 0.0f;
        float  z = 0.0f;
    };

    struct Spike {
        double t = 0.0;
        float  dpos_m = 0.0f;
        float  dt_s = 0.0f;
        float  from_x = 0.0f;
        float  from_y = 0.0f;
        float  from_z = 0.0f;
        float  to_x = 0.0f;
        float  to_y = 0.0f;
        float  to_z = 0.0f;
    };

    struct Track {
        std::uint64_t                uid = 0;
        int                          kind = 0;
        std::array<Sample, kRingSize> ring{};
        std::size_t                  head = 0;
        std::size_t                  count = 0;
        float                        peak_speed = 0.0f;
        float                        p95_speed = 0.0f;
        float                        median_speed = 0.0f;
        std::deque<Spike>            spikes;
        float                        last_x = 0.0f;
        float                        last_y = 0.0f;
        float                        last_z = 0.0f;
        double                       last_t = 0.0;
        bool                         seeded = false;
        double                       last_seen = 0.0;
    };

    std::unordered_map<std::uint64_t, Track> tracks_;
    std::uint64_t                            selected_uid_ = 0;
    double                                   last_processed_t_ = 0.0;
    float                                    threshold_m_s_ = 5.0f;
    float                                    spike_dpos_m_ = 2.0f;
    bool                                     log_anomalies_ = false;

    void ingest_snapshot();
    void recompute_aggregates(Track& t);
    static void ordered_speeds(const Track& t, std::vector<float>& out);
    static void draw_histogram(const Track& t, float threshold_m_s);
};

}  // namespace dxs
