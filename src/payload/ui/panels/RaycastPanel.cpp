#include "RaycastPanel.hpp"

#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/panels/EntitiesPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

namespace dxs {

namespace {

bool parse_pos(const std::string& extras, float& x, float& y, float& z) {
    const auto p = extras.find("pos=(");
    if (p == std::string::npos) return false;
    return std::sscanf(extras.c_str() + p + 5, "%f,%f,%f", &x, &y, &z) == 3;
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

    ImGui::Dummy(ImVec2(0, 10));

    // Build the request list: every entity the EntitiesPanel knows about
    // gets its world position projected on the sampler's next tick. Next
    // frame the result is in snap.screen[uid]. This is the whole ESP loop:
    // one read-modify-request per frame, no per-entity Python calls.
    const EntitiesPanel* ep = EntitiesPanel::global();
    std::vector<std::pair<std::uint64_t, Vec3>> request;
    if (ep) {
        request.reserve(ep->rows().size());
        std::uint64_t synth_uid = 1;
        for (const auto& r : ep->rows()) {
            float px, py, pz;
            if (!parse_pos(r.extras, px, py, pz)) continue;
            // EntitiesPanel rows don't all carry a numeric uid — synthesise
            // a stable one from insertion order so Snapshot::screen keys
            // line up with the same rows next frame.
            request.emplace_back(synth_uid++, Vec3{ px, py, pz });
        }
        sampler.request_world_to_screen(request);
    }

    // Rank the entities by angular offset from the view forward — this
    // answers "what am I looking at" without needing an actual raycast
    // (the engine raycast primitive isn't located yet).
    struct Row {
        std::string kind;
        std::string name;
        float       angle_deg;
        float       dist;
        Vec3        pos;
    };
    std::vector<Row> ranked;
    if (ep) {
        for (const auto& r : ep->rows()) {
            float px, py, pz;
            if (!parse_pos(r.extras, px, py, pz)) continue;
            Vec3 d{ px - snap.cam_pos.x, py - snap.cam_pos.y, pz - snap.cam_pos.z };
            const float len = std::sqrt(dot3(d, d));
            if (len < 1e-3f) continue;
            Vec3 dn{ d.x / len, d.y / len, d.z / len };
            const float cos_a = dot3(dn, snap.cam_forward);
            const float ang = std::acos(std::clamp(cos_a, -1.0f, 1.0f)) * 57.29578f;
            Row row;
            row.kind      = r.kind;
            row.name      = r.cls;
            row.angle_deg = ang;
            row.dist      = len;
            row.pos       = Vec3{ px, py, pz };
            ranked.push_back(std::move(row));
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const Row& a, const Row& b) { return a.angle_deg < b.angle_deg; });
    }

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("closest to crosshair (by angular offset)");
    ImGui::PopStyleColor();

    if (ImGui::BeginTable("raycast_table", 5,
                          ImGuiTableFlags_BordersInner |
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("kind");
        ImGui::TableSetupColumn("name");
        ImGui::TableSetupColumn("angle");
        ImGui::TableSetupColumn("dist");
        ImGui::TableSetupColumn("screen");
        ImGui::TableHeadersRow();

        const int cap = std::min<int>(8, static_cast<int>(ranked.size()));
        for (int i = 0; i < cap; ++i) {
            const auto& r = ranked[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(r.kind.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(r.name.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f°", r.angle_deg);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f",  r.dist);
            ImGui::TableSetColumnIndex(4);
            // Screen coords arrive on the NEXT tick after we request them —
            // on startup the first frame shows "—".
            const auto it = snap.screen.find(static_cast<std::uint64_t>(i + 1));
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
