#include "RaycastPanel.hpp"

#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/framework/View3D.hpp"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace dxs {

namespace {

view3d::ImVec3 to_world_m(Vec3 v) {
    return {
        v.x * theme::world_to_meter,
        v.y * theme::world_to_meter,
        v.z * theme::world_to_meter,
    };
}

view3d::ImVec3 to_dir(Vec3 v) {
    return {v.x, v.y, v.z};
}

ImU32 unit_color(int kind) {
    switch (kind) {
        case 1: return theme::to_u32(theme::bad);
        case 2: return theme::to_u32(theme::good);
        default: return theme::to_u32(theme::on_surface_muted);
    }
}

void draw_center_label(const view3d::Canvas& canvas, const char* label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 text_sz = ImGui::CalcTextSize(label);
    dl->AddText(
        ImVec2(
            canvas.tl.x + (canvas.size.x - text_sz.x) * 0.5f,
            canvas.tl.y + (canvas.size.y - text_sz.y) * 0.5f),
        theme::to_u32(theme::on_surface_muted),
        label);
}

}  // namespace

void RaycastPanel::draw() {
    auto snap = CameraSampler::instance().snapshot();
    static view3d::OrbitState orbit{};
    static bool orbit_seeded = false;
    if (snap.camera_ready && !orbit_seeded) {
        orbit.center = to_world_m(snap.cam_pos);
        orbit_seeded = true;
    }

    const ImVec2 avail  = ImGui::GetContentRegionAvail();
    const float  gap    = theme::space_lg;
    const float  left_w = std::max(1.0f, (avail.x - gap) * 0.6f);

    if (ImGui::BeginChild("##raycast_left", ImVec2(left_w, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse)) {
        const float caption_h = ImGui::GetTextLineHeightWithSpacing();
        const float canvas_h = std::max(
            140.0f, ImGui::GetContentRegionAvail().y - caption_h - theme::space_sm);
        auto canvas = view3d::begin("##raycast_canvas", ImVec2(0.0f, canvas_h), &orbit);
        view3d::draw_grid_xz(canvas, 30.0f, 2.0f);
        view3d::draw_axes(canvas);

        if (snap.camera_ready) {
            for (const auto& unit : snap.units) {
                view3d::draw_point(canvas, to_world_m(unit.pos), 3.0f, unit_color(unit.kind));
            }
            const view3d::ImVec3 cam_pos = to_world_m(snap.cam_pos);
            view3d::draw_point(canvas, cam_pos, 4.0f, IM_COL32(255, 255, 255, 255));
            view3d::draw_line(
                canvas,
                cam_pos,
                cam_pos + to_dir(snap.cam_forward) * 20.0f,
                IM_COL32(255, 255, 255, 180),
                1.8f);
        } else {
            draw_center_label(canvas, "NO CAMERA DATA");
        }
        view3d::end(canvas);

        ImGui::Dummy(ImVec2(0.0f, theme::space_xs));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::Text("yaw %.2f  pitch %.2f  dist %.2fm",
                    orbit.yaw, orbit.pitch, orbit.dist);
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);

    if (ImGui::BeginChild("##raycast_right", ImVec2(0.0f, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar)) {
        theme::section_divider("RAY");
        ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
        if (snap.camera_ready &&
            ImGui::BeginTable("##raycast_ray", 2,
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 78.0f);
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

            row("origin", "(%+.2f, %+.2f, %+.2f)",
                snap.cam_pos.x * theme::world_to_meter,
                snap.cam_pos.y * theme::world_to_meter,
                snap.cam_pos.z * theme::world_to_meter);
            row("direction", "(%+.3f, %+.3f, %+.3f)",
                snap.cam_forward.x, snap.cam_forward.y, snap.cam_forward.z);
            ImGui::EndTable();
        } else if (!snap.camera_ready) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("unavailable");
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, theme::space_md));
        theme::section_divider("HIT");
        ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("no hit");
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

}  // namespace dxs
