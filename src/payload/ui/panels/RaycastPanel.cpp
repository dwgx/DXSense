#include "RaycastPanel.hpp"

#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <vector>

#include <imgui.h>

namespace dxs {

namespace {

const char* kind_name(int k) {
    switch (k) {
        case 1:   return "BUTCHER";
        case 2:   return "CIVILIAN";
        case 3:   return "GENERATOR";
        case 4:   return "HOOK";
        case 5:   return "BOX";
        case 6:   return "DOOR";
        case 7:   return "WOOD";
        case 8:   return "PANEL";
        case 9:   return "CUPBOARD";
        case 10:  return "CAVE";
        case 11:  return "CROW";
        case 12:  return "SWITCH";
        case 510: return "SPIRIT";
        default:  return "?";
    }
}

float dot3(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

}  // namespace

void RaycastPanel::draw() {
    auto& sampler = CameraSampler::instance();
    auto  snap    = sampler.snapshot();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "Camera-origin line-of-sight probe. Uses the live view basis from "
        "CameraSampler (forward / right / up) and the engine's own "
        "world->screen projection via cam.world_to_screen.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    if (!snap.camera_ready) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::TextUnformatted("no live camera — enter a 3D scene.");
        ImGui::PopStyleColor();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("view basis (world space)");
    ImGui::PopStyleColor();
    ImGui::Text("  origin   = (%+.2f, %+.2f, %+.2f)",
                snap.cam_pos.x, snap.cam_pos.y, snap.cam_pos.z);
    ImGui::Text("  forward  = (%+.3f, %+.3f, %+.3f)",
                snap.cam_forward.x, snap.cam_forward.y, snap.cam_forward.z);
    ImGui::Text("  right    = (%+.3f, %+.3f, %+.3f)",
                snap.cam_right.x, snap.cam_right.y, snap.cam_right.z);
    ImGui::Text("  up       = (%+.3f, %+.3f, %+.3f)",
                snap.cam_up.x, snap.cam_up.y, snap.cam_up.z);
    ImGui::Text("  units    = %d live   ·   fov %.1f°",
                static_cast<int>(snap.units.size()), snap.fov_y);

    ImGui::Dummy(ImVec2(0, 10));

    // Request world->screen projection for every live unit — next tick the
    // result shows up in snap.screen[uid]. This is the whole ESP loop: one
    // read-modify-request per frame, no per-unit Python calls.
    std::vector<std::pair<std::uint64_t, Vec3>> request;
    request.reserve(snap.units.size());
    for (const auto& u : snap.units) request.emplace_back(u.uid, u.pos);
    sampler.request_world_to_screen(std::move(request));

    // Rank units by angular offset from view forward — "what am I looking at"
    // without needing an actual engine raycast primitive.
    struct Row {
        std::uint64_t uid;
        int           kind;
        float         angle_deg;
        float         dist;
    };
    std::vector<Row> ranked;
    ranked.reserve(snap.units.size());
    for (const auto& u : snap.units) {
        Vec3 d{ u.pos.x - snap.cam_pos.x,
                u.pos.y - snap.cam_pos.y,
                u.pos.z - snap.cam_pos.z };
        const float len = std::sqrt(dot3(d, d));
        if (len < 1e-3f) continue;
        Vec3 dn{ d.x / len, d.y / len, d.z / len };
        const float cos_a = dot3(dn, snap.cam_forward);
        const float ang   = std::acos(std::clamp(cos_a, -1.0f, 1.0f)) * 57.29578f;
        ranked.push_back(Row{ u.uid, u.kind, ang, len });
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const Row& a, const Row& b) { return a.angle_deg < b.angle_deg; });

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("closest to crosshair (angular offset)");
    ImGui::PopStyleColor();

    if (ImGui::BeginTable("raycast_table", 5,
                          ImGuiTableFlags_BordersInner |
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("kind");
        ImGui::TableSetupColumn("uid");
        ImGui::TableSetupColumn("angle");
        ImGui::TableSetupColumn("dist");
        ImGui::TableSetupColumn("screen");
        ImGui::TableHeadersRow();

        const int cap = std::min<int>(12, static_cast<int>(ranked.size()));
        for (int i = 0; i < cap; ++i) {
            const auto& r = ranked[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(kind_name(r.kind));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%llu", static_cast<unsigned long long>(r.uid));
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f°", r.angle_deg);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f",  r.dist);
            ImGui::TableSetColumnIndex(4);
            const auto it = snap.screen.find(r.uid);
            if (it != snap.screen.end()) {
                ImGui::Text("%.0f, %.0f", it->second.first, it->second.second);
            } else {
                ImGui::TextUnformatted("—");
            }
        }
        ImGui::EndTable();
    }
}

}  // namespace dxs
