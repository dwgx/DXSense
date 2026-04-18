#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dxs {

class VulnLabPanel : public IPanel {
public:
    VulnLabPanel();
    std::string_view id()       const override { return "vuln_lab"; }
    std::string_view category() const override { return L("sidebar.lab"); }
    std::string_view title()    const override { return L("panel.vuln_lab.title"); }
    std::string_view icon()     const override { return ICON_WARNING; }
    void             draw()           override;
    using Pair = std::pair<std::string, std::string>;
    struct CfgTuple {
        int sid = 0, flag = 0, default_flag = 0;
        bool present = false, default_present = false;
        float delta = 0.0f, duration = 0.0f, default_delta = 0.0f, default_duration = 0.0f;
    };
    struct State {
        bool has_me = false, in_battle = false, msm_alive = false, has_force_speed = false;
        bool nohit_has_per = false, nohit_tag = false, armed = false;
        std::string role;
        std::uint64_t uid = 0;
        float speed = 0.0f, speed_real = 0.0f, force_speed = 0.0f, gm_speed = -1.0f;
        float super_scale = 0.0f, super_duration = 0.0f, nohit_per = 0.0f;
        int anti_info_count = 0, anti_handler_count = 0;
        std::vector<Pair> anti_infos;
        std::vector<int> passives;
        std::array<CfgTuple, 6> cfgs{};
        std::string last_applied_summary;
    };
    struct Knob {
        bool armed = false, anti_clear = true, super_atk_on = false, nohit_on = false;
        bool nohit_tag = true, cocoon_on = false, hide_on = false;
        float speed_target = 150.0f;
        float super_scale = 2.0f, super_duration = 10.0f, nohit_per = 0.5f;
        float cocoon_val = 0.0f, hide_val = 0.0f;
    };

private:
    bool ensure_helper_installed();
    bool exec_json_command(std::string_view source, std::string& json_out);
    bool exec_apply(std::string_view label, std::string_view source);
    void restore_knob();
    void persist_knob() const;
    bool push_knob_to_helper();
    void parse_state_json(std::string_view json, State& out, bool update_applied_summary) const;
    void poll_state(bool force = false);
    void run_watchdog(bool force = false);
    void seed_cfg_inputs_from_selected();
    void on_knob_changed(bool force_watchdog);
    void panic_off();
    State state_{};
    Knob  knob_{};
    bool helper_installed_ = false, helper_toasted_ = false, read_failed_ = false, cfg_inputs_seeded_ = false;
    std::string read_error_;
    double last_install_attempt_ = 0.0, last_watchdog_at_ = -1.0, last_read_at_ = -1.0;
    int cfg_selected_index_ = 0, cfg_flag_input_ = 0;
    float cfg_delta_input_ = 0.0f, cfg_duration_input_ = 0.0f;
    int add_skill_sid_input_ = 137, add_skill_lv_input_ = 1;
    static constexpr double k_watchdog_interval = 0.30;
    static constexpr double k_read_interval = 0.50;
};

}  // namespace dxs
