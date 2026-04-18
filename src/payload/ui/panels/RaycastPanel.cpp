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
        case 1:    return "BUTCHER";
        case 2:    return "CIVILIAN";
        case 3:    return "GENERATOR";
        case 4:    return "HOOK";
        case 5:    return "BOX";
        case 6:    return "DOOR";
        case 7:    return "WOOD";
        case 8:    return "PANEL";
        case 9:    return "CUPBOARD";
        case 10:   return "CAVE";
        case 11:   return "CROW";
        case 12:   return "SWITCH";
        case 53:   return "LOBBY_PROP";
        case 59:   return "LOBBY_PROP";
        case 74:   return "LOBBY_PROP";
        case 100:  return "LOBBY_CHAR";
        case 510:  return "SPIRIT";
        case 1009: return "LOBBY_NPC";
        default:   return "?";
    }
}

float dot3(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

}  // namespace

// ---------------------------------------------------------------------------
// Raycast panel — flat ChatGPT-style layout. No floating inner cards, no
// hard-coded heights; content flows naturally so section labels and badges
// never overlap the content above them. Vectors render as a two-column
// grid, the ranked crosshair candidates land in a single plain table.
// ---------------------------------------------------------------------------
void RaycastPanel::draw() {
    auto snap = CameraSampler::instance().snapshot();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextWrapped(
        "Camera-origin line-of-sight probe. Uses the live view basis from "
        "CameraSampler and the engine's world_to_screen projection for the "
        "screen column.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, theme::space_md));

    if (!snap.camera_ready) {
        theme::badge(theme::Status::Warn, "No live camera — enter a 3D scene");
        return;
    }

    // --- View basis readout (flat two-column) -----------------------------
    theme::section_label("VIEW BASIS", "world space");
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    if (ImGui::BeginTable("##basis", 2,
                          ImGuiTableFlags_SizingFixedFit |
                          ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

        auto row = [](const char* label, const char* fmt, auto... args) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(fmt, args...);
        };
        row("origin",  "(%+.2f, %+.2f, %+.2f)",
            snap.cam_pos.x, snap.cam_pos.y, snap.cam_pos.z);
        row("forward", "(%+.3f, %+.3f, %+.3f)",
            snap.cam_forward.x, snap.cam_forward.y, snap.cam_forward.z);
        row("right",   "(%+.3f, %+.3f, %+.3f)",
            snap.cam_right.x, snap.cam_right.y, snap.cam_right.z);
        row("up",      "(%+.3f, %+.3f, %+.3f)",
            snap.cam_up.x, snap.cam_up.y, snap.cam_up.z);
        row("fov",     "%.1f°",    snap.fov_y);
        ImGui::EndTable();
    }

    auto is_real = [](const Vec3& p) {
        return std::fabs(p.x) + std::fabs(p.y) + std::fabs(p.z) > 0.01f;
    };
    int real_count = 0;
    for (const auto& u : snap.units) if (is_real(u.pos)) ++real_count;

    // Plain count line beneath the basis table.
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::Text("%d units live  ·  %d positioned",
                static_cast<int>(snap.units.size()), real_count);
    ImGui::PopStyleColor();

    if (!snap.in_battle) {
        ImGui::Dummy(ImVec2(0, theme::space_sm));
        theme::badge(theme::Status::Idle, "Lobby scene — enter a match for useful data");
    }

    ImGui::Dummy(ImVec2(0, theme::space_lg));

    // --- Candidates table -------------------------------------------------
    theme::section_label("CROSSHAIR CANDIDATES", "closest to forward");
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    struct Row {
        std::uint64_t uid;
        int           kind;
        float         angle_deg;
        float         dist;
        bool          has_screen;
        float         sx, sy;
    };
    std::vector<Row> ranked;
    ranked.reserve(real_count);
    for (const auto& u : snap.units) {
        if (!is_real(u.pos)) continue;
        Vec3 d{ u.pos.x - snap.cam_pos.x,
                u.pos.y - snap.cam_pos.y,
                u.pos.z - snap.cam_pos.z };
        const float len = std::sqrt(dot3(d, d));
        if (len < 1e-3f) continue;
        Vec3 dn{ d.x / len, d.y / len, d.z / len };
        const float cos_a = dot3(dn, snap.cam_forward);
        const float ang   = std::acos(std::clamp(cos_a, -1.0f, 1.0f)) * 57.29578f;
        const auto sp = snap.project(u.pos, viewport.x, viewport.y);
        ranked.push_back(Row{
            u.uid, u.kind, ang,
            len * theme::world_to_meter,   // display distance in metres
            sp.has_value(),
            sp ? sp->first  : 0.0f,
            sp ? sp->second : 0.0f,
        });
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const Row& a, const Row& b) { return a.angle_deg < b.angle_deg; });

    if (ranked.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No positioned entities in view.");
        ImGui::PopStyleColor();
        return;
    }

    if (ImGui::BeginTable("raycast_table", 5,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_BordersInnerH |
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
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::Text("%llu", static_cast<unsigned long long>(r.uid));
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f°", r.angle_deg);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.1fm",  r.dist);
            ImGui::TableSetColumnIndex(4);
            if (r.has_screen) {
                ImGui::Text("%.0f, %.0f", r.sx, r.sy);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_disabled);
                ImGui::TextUnformatted("—");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
}

}  // namespace dxs
