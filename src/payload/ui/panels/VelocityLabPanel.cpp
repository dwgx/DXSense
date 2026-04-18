#include "VelocityLabPanel.hpp"

#include "core/EventLog.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace dxs {

namespace {

constexpr double k_track_ttl_s   = 30.0;
constexpr float  k_spike_dt_s    = 0.5f;
constexpr int    k_hist_bins     = 20;
constexpr size_t k_max_spikes    = 50;
constexpr float  k_small_dt_eps  = 1e-4f;

std::string_view kind_label(int kind) {
    switch (kind) {
        case 1:    return L("esp.kind_hunter");
        case 2:    return L("esp.kind_survivor");
        case 3:    return L("esp.kind_generator");
        case 4:    return L("esp.kind_hook");
        case 5:    return L("esp.kind_box");
        case 6:    return L("esp.kind_door");
        case 7:    return L("esp.kind_pallet");
        case 8:    return L("esp.kind_panel");
        case 9:    return L("esp.kind_cupboard");
        case 10:   return L("esp.kind_basement");
        case 11:   return L("esp.kind_crow");
        case 12:   return L("esp.kind_switch");
        case 18:   return L("esp.kind_hatch");
        case 100:  return L("esp.kind_lobby");
        case 222:  return L("esp.kind_item");
        case 510:  return L("esp.kind_spirit");
        case 1009: return L("esp.kind_npc");
        case 1911: return L("esp.kind_marker");
        default:   return L("esp.kind_unknown");
    }
}

void draw_metric_tile(const char* id,
                      std::string_view label,
                      float value,
                      const ImVec4& value_color,
                      float width) {
    ImGui::PushID(id);
    const ImVec2 size(width, 76.0f);
    const ImVec2 tl = ImGui::GetCursorScreenPos();
    const ImVec2 br = tl + size;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    theme::draw_surface(dl, tl, br, theme::radius_md, theme::surface_ctn_low, nullptr, 0.0f, false);

    ImGui::SetCursorScreenPos(tl + ImVec2(theme::space_md, theme::space_sm));
    theme::section_label(label);

    char value_buf[32];
    std::snprintf(value_buf, sizeof(value_buf), "%.2f", value);
    ImGui::SetCursorScreenPos(tl + ImVec2(theme::space_md, 28.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, value_color);
    ImGui::SetWindowFontScale(theme::scale_metric);
    ImGui::TextUnformatted(value_buf);
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(tl);
    ImGui::Dummy(size);
    ImGui::PopID();
}


}  // namespace

void VelocityLabPanel::ingest_snapshot() {
    const auto snap = CameraSampler::instance().snapshot();
    if (snap.sample_time <= 0.0 || snap.sample_time == last_processed_t_) return;

    for (const auto& u : snap.units) {
        Track& t = tracks_[u.uid];
        t.uid = u.uid;
        t.kind = u.kind;
        t.last_seen = snap.sample_time;

        if (!t.seeded) {
            t.last_x = u.pos.x;
            t.last_y = u.pos.y;
            t.last_z = u.pos.z;
            t.last_t = snap.sample_time;
            t.seeded = true;
            continue;
        }

        const float dx = (u.pos.x - t.last_x) * theme::world_to_meter;
        const float dy = (u.pos.y - t.last_y) * theme::world_to_meter;
        const float dz = (u.pos.z - t.last_z) * theme::world_to_meter;
        const float dpos = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float dt = static_cast<float>(snap.sample_time - t.last_t);
        const float speed = dt > k_small_dt_eps ? (dpos / dt) : 0.0f;

        t.ring[t.head] = Sample{
            snap.sample_time,
            speed,
            u.pos.x * theme::world_to_meter,
            u.pos.y * theme::world_to_meter,
            u.pos.z * theme::world_to_meter,
        };
        t.head = (t.head + 1) % kRingSize;
        t.count = std::min(t.count + 1, kRingSize);

        const bool spike = dt > k_small_dt_eps && dt < k_spike_dt_s && dpos > spike_dpos_m_;
        if (spike) {
            t.spikes.push_back(Spike{
                snap.sample_time,
                dpos,
                dt,
                t.last_x * theme::world_to_meter,
                t.last_y * theme::world_to_meter,
                t.last_z * theme::world_to_meter,
                u.pos.x * theme::world_to_meter,
                u.pos.y * theme::world_to_meter,
                u.pos.z * theme::world_to_meter,
            });
            if (t.spikes.size() > k_max_spikes) t.spikes.pop_front();

            if (log_anomalies_ && EventLog::instance().enabled()) {
                char body[384];
                std::snprintf(body, sizeof(body),
                    "\"uid\":%llu,\"kind\":%d,\"dt_s\":%.4f,\"dpos_m\":%.3f,"
                    "\"from\":[%.3f,%.3f,%.3f],\"to\":[%.3f,%.3f,%.3f]",
                    static_cast<unsigned long long>(u.uid), u.kind, dt, dpos,
                    t.last_x * theme::world_to_meter,
                    t.last_y * theme::world_to_meter,
                    t.last_z * theme::world_to_meter,
                    u.pos.x * theme::world_to_meter,
                    u.pos.y * theme::world_to_meter,
                    u.pos.z * theme::world_to_meter);
                EventLog::instance().emit("velocity_spike", body);
            }
        }

        t.last_x = u.pos.x;
        t.last_y = u.pos.y;
        t.last_z = u.pos.z;
        t.last_t = snap.sample_time;
    }

    for (auto it = tracks_.begin(); it != tracks_.end(); ) {
        if (snap.sample_time - it->second.last_seen > k_track_ttl_s) {
            if (selected_uid_ == it->first) selected_uid_ = 0;
            it = tracks_.erase(it);
        } else {
            recompute_aggregates(it->second);
            ++it;
        }
    }

    last_processed_t_ = snap.sample_time;
}

void VelocityLabPanel::recompute_aggregates(Track& t) {
    if (t.count == 0) {
        t.peak_speed = 0.0f;
        t.p95_speed = 0.0f;
        t.median_speed = 0.0f;
        return;
    }

    std::vector<float> speeds;
    speeds.reserve(t.count);
    for (std::size_t i = 0; i < t.count; ++i) speeds.push_back(t.ring[i].speed_m_s);
    std::sort(speeds.begin(), speeds.end());

    const std::size_t mid = speeds.size() / 2;
    const std::size_t p95 = std::min(speeds.size() - 1,
                                     static_cast<std::size_t>(0.95 * speeds.size()));
    t.median_speed = speeds[mid];
    t.p95_speed = speeds[p95];
    t.peak_speed = speeds.back();
}

void VelocityLabPanel::ordered_speeds(const Track& t, std::vector<float>& out) {
    out.clear();
    out.reserve(t.count);
    if (t.count == 0) return;

    const std::size_t start = (t.head + kRingSize - t.count) % kRingSize;
    for (std::size_t i = 0; i < t.count; ++i) {
        out.push_back(t.ring[(start + i) % kRingSize].speed_m_s);
    }
}

void VelocityLabPanel::draw_histogram(const Track& t, float threshold_m_s) {
    theme::section_label("HISTOGRAM", "speed distribution");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    const ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size(ImGui::GetContentRegionAvail().x, 110.0f);
    const ImVec2 canvas_end  = canvas_pos + canvas_size;
    ImDrawList*  dl          = ImGui::GetWindowDrawList();
    theme::draw_surface(dl, canvas_pos, canvas_end, theme::radius_md,
                        theme::surface_ctn_low, nullptr, 0.0f, false);

    std::vector<float> speeds;
    ordered_speeds(t, speeds);
    if (canvas_size.x < 40.0f || canvas_size.y < 40.0f || speeds.empty()) {
        dl->AddText(canvas_pos + ImVec2(theme::space_md, theme::space_md),
                    theme::to_u32(theme::on_surface_muted), "No speed samples yet.");
        ImGui::Dummy(canvas_size);
        return;
    }

    std::array<int, k_hist_bins> bins{};
    const float max_speed = std::max({t.peak_speed, threshold_m_s, 1.0f});
    int max_bin = 1;
    for (float speed : speeds) {
        const int bin = std::clamp(static_cast<int>((speed / max_speed) * k_hist_bins),
                                   0, k_hist_bins - 1);
        ++bins[bin];
        max_bin = std::max(max_bin, bins[bin]);
    }

    const float pad = theme::space_md;
    const ImVec2 plot_min = canvas_pos + ImVec2(pad, pad);
    const ImVec2 plot_max = canvas_end - ImVec2(pad, pad + theme::font_caption);
    const float plot_w = plot_max.x - plot_min.x;
    const float plot_h = plot_max.y - plot_min.y;
    const float gap = 3.0f;
    const float bar_w = (plot_w - gap * (k_hist_bins - 1)) / k_hist_bins;

    for (int i = 0; i < k_hist_bins; ++i) {
        const float ratio = static_cast<float>(bins[i]) / static_cast<float>(max_bin);
        const float h = ratio * plot_h;
        const float x0 = plot_min.x + i * (bar_w + gap);
        const float x1 = x0 + bar_w;
        const float y1 = plot_max.y;
        const float y0 = y1 - h;
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                          theme::to_u32(theme::with_alpha(theme::on_surface, 0.85f)),
                          3.0f);
    }

    char right_buf[16];
    std::snprintf(right_buf, sizeof(right_buf), "%.1f", max_speed);
    dl->AddText(ImVec2(plot_min.x, canvas_end.y - theme::font_caption - 6.0f),
                theme::to_u32(theme::on_surface_muted), "0.0");
    const ImVec2 right_size = ImGui::CalcTextSize(right_buf);
    dl->AddText(ImVec2(plot_max.x - right_size.x, canvas_end.y - theme::font_caption - 6.0f),
                theme::to_u32(theme::on_surface_muted), right_buf);

    ImGui::Dummy(canvas_size);
}

void VelocityLabPanel::draw() {
    bool log_anomalies = log_anomalies_;
    if (theme::toggle("velocity_lab.log", &log_anomalies)) log_anomalies_ = log_anomalies;
    ImGui::SameLine(0, theme::space_sm);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextUnformatted(L("velocity_lab.log_anomalies").data());
    ImGui::PopStyleColor();

    ImGui::SameLine(0, theme::space_xl);
    ImGui::SetNextItemWidth(210.0f);
    ImGui::SliderFloat(L("velocity_lab.threshold").data(), &threshold_m_s_, 1.0f, 20.0f, "%.1f");

    ImGui::SameLine(0, theme::space_xl);
    ImGui::SetNextItemWidth(210.0f);
    ImGui::SliderFloat(L("velocity_lab.spike_dpos").data(), &spike_dpos_m_, 0.5f, 10.0f, "%.1f");

    ImGui::Dummy(ImVec2(0, theme::space_md));
    ingest_snapshot();

    if (tracks_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted(L("velocity_lab.empty").data());
        ImGui::PopStyleColor();
        return;
    }

    std::vector<Track*> sorted_tracks;
    sorted_tracks.reserve(tracks_.size());
    for (auto& [uid, track] : tracks_) {
        (void)uid;
        sorted_tracks.push_back(&track);
    }
    std::sort(sorted_tracks.begin(), sorted_tracks.end(),
        [](const Track* a, const Track* b) {
            if (a->peak_speed != b->peak_speed) return a->peak_speed > b->peak_speed;
            return a->uid < b->uid;
        });

    if (ImGui::BeginTable("##velocity_lab_split", 2,
                          ImGuiTableFlags_Resizable |
                          ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed, 280.0f);
        ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        theme::section_label("TRACKS", "sorted by peak speed");
        ImGui::Dummy(ImVec2(0, theme::space_xs));
        if (ImGui::BeginTable("##velocity_lab_tracks", 5,
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingFixedFit,
                              ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("kind",   ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("uid",    ImGuiTableColumnFlags_WidthFixed, 82.0f);
            ImGui::TableSetupColumn("peak",   ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("p95",    ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("spikes", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableHeadersRow();

            for (const Track* t : sorted_tracks) {
                const bool selected = selected_uid_ == t->uid;
                const bool hot = t->peak_speed > threshold_m_s_;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (hot) ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
                if (ImGui::Selectable(("##track" + std::to_string(t->uid)).c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_uid_ = t->uid;
                }
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(kind_label(t->kind).data());
                if (hot) ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                if (hot) ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
                ImGui::Text("%llu", static_cast<unsigned long long>(t->uid));
                if (hot) ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                if (hot) ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
                ImGui::Text("%.1f", t->peak_speed);
                if (hot) ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(3);
                if (hot) ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
                ImGui::Text("%.1f", t->p95_speed);
                if (hot) ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(4);
                if (hot) ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
                ImGui::Text("%d", static_cast<int>(t->spikes.size()));
                if (hot) ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        ImGui::TableSetColumnIndex(1);
        auto it = tracks_.find(selected_uid_);
        if (it == tracks_.end()) {
            theme::section_label("DETAIL");
            ImGui::Dummy(ImVec2(0, theme::space_sm));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted(L("velocity_lab.select_hint").data());
            ImGui::PopStyleColor();
            ImGui::EndTable();
            return;
        }

        Track& t = it->second;
        const float avail_w = ImGui::GetContentRegionAvail().x;
        theme::section_label("SELECTED TRACK");
        ImGui::Dummy(ImVec2(0, theme::space_xs));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::Text("%s  |  uid %llu",
                    kind_label(t.kind).data(),
                    static_cast<unsigned long long>(t.uid));
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, theme::space_md));
        const float tile_w = (avail_w - theme::space_md * 2.0f) / 3.0f;
        draw_metric_tile("peak", "PEAK", t.peak_speed,
                         t.peak_speed > threshold_m_s_ ? theme::bad : theme::on_surface, tile_w);
        ImGui::SameLine(0, theme::space_md);
        draw_metric_tile("p95", "P95", t.p95_speed, theme::on_surface, tile_w);
        ImGui::SameLine(0, theme::space_md);
        draw_metric_tile("median", "MEDIAN", t.median_speed, theme::on_surface, tile_w);

        std::vector<float> speeds;
        ordered_speeds(t, speeds);

        ImGui::Dummy(ImVec2(0, theme::space_md));
        theme::section_label("SPEED SERIES", "meters / second");
        ImGui::Dummy(ImVec2(0, theme::space_xs));
        const float plot_max = std::max({t.peak_speed, threshold_m_s_, 1.0f}) * 1.2f;
        ImGui::PlotLines("##velocity_lab_speed", speeds.data(),
                         static_cast<int>(speeds.size()), 0, nullptr,
                         0.0f, plot_max,
                         ImVec2(ImGui::GetContentRegionAvail().x, 100.0f));
        if (!speeds.empty()) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float ratio = std::clamp(threshold_m_s_ / plot_max, 0.0f, 1.0f);
            const float y = max.y - (max.y - min.y) * ratio;
            dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), theme::to_u32(theme::warn), 1.5f);
            char th_buf[32];
            std::snprintf(th_buf, sizeof(th_buf), "threshold %.1f", threshold_m_s_);
            dl->AddText(ImVec2(min.x + theme::space_sm, y - theme::font_caption - 2.0f),
                        theme::to_u32(theme::warn), th_buf);
        }

        ImGui::Dummy(ImVec2(0, theme::space_md));
        draw_histogram(t, threshold_m_s_);

        ImGui::Dummy(ImVec2(0, theme::space_md));
        theme::section_label("SPIKES", "dpos > threshold and dt < 0.5 s");
        ImGui::Dummy(ImVec2(0, theme::space_xs));
        if (ImGui::BeginTable("##velocity_lab_spikes", 4,
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingStretchProp,
                              ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("dt", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("dpos", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("from -> to", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto spike_it = t.spikes.rbegin(); spike_it != t.spikes.rend(); ++spike_it) {
                const Spike& s = *spike_it;
                char time_buf[32];
                char from_to_buf[160];
                std::snprintf(time_buf, sizeof(time_buf), "%.2fs", s.t - last_processed_t_);
                std::snprintf(from_to_buf, sizeof(from_to_buf),
                    "(%.1f, %.1f, %.1f) -> (%.1f, %.1f, %.1f)",
                    s.from_x, s.from_y, s.from_z, s.to_x, s.to_y, s.to_z);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
                ImGui::TextUnformatted(time_buf);
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", s.dt_s);

                ImGui::TableSetColumnIndex(2);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
                ImGui::Text("%.2f m", s.dpos_m);
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(3);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
                ImGui::TextUnformatted(from_to_buf);
                ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }

        ImGui::EndTable();
    }
}

}  // namespace dxs
