#include "MatrixPanel.hpp"

#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/framework/View3D.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace dxs {

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

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

void draw_matrix_grid(const float m[16], bool active_text) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 4.0f));
    if (ImGui::BeginTable("##matrix_view", 4,
                          ImGuiTableFlags_BordersInner |
                          ImGuiTableFlags_SizingStretchSame)) {
        for (int r = 0; r < 4; ++r) {
            ImGui::TableNextRow();
            for (int c = 0; c < 4; ++c) {
                ImGui::TableSetColumnIndex(c);
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    active_text ? theme::on_surface_variant : theme::on_surface_muted);
                ImGui::Text("%+7.2f", m[r * 4 + c]);
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
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

void draw_vec_row(const char* label, Vec3 v, bool meters) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::TableSetColumnIndex(1);
    const float s = meters ? theme::world_to_meter : 1.0f;
    ImGui::Text("(%+.2f, %+.2f, %+.2f)", v.x * s, v.y * s, v.z * s);
}

}  // namespace

void MatrixPanel::draw() {
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

    if (ImGui::BeginChild("##matrix_left", ImVec2(left_w, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse)) {
        const float caption_h = ImGui::GetTextLineHeightWithSpacing();
        const float canvas_h = std::max(
            140.0f, ImGui::GetContentRegionAvail().y - caption_h - theme::space_sm);
        auto canvas = view3d::begin("##matrix_canvas", ImVec2(0.0f, canvas_h), &orbit);
        view3d::draw_grid_xz(canvas, 30.0f, 2.0f);
        view3d::draw_axes(canvas);

        if (snap.camera_ready) {
            for (const auto& unit : snap.units) {
                view3d::draw_point(canvas, to_world_m(unit.pos), 3.0f, unit_color(unit.kind));
            }
            view3d::draw_frustum(
                canvas,
                to_world_m(snap.cam_pos),
                to_dir(snap.cam_forward),
                to_dir(snap.cam_up),
                snap.fov_y * kDegToRad,
                16.0f / 9.0f,
                0.3f,
                30.0f,
                theme::to_u32(theme::info));
            if (snap.player_ready) {
                view3d::draw_line(
                    canvas,
                    to_world_m(snap.player_pos),
                    to_world_m(snap.cam_pos),
                    IM_COL32(255, 255, 255, 72),
                    1.0f);
            }
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

    if (ImGui::BeginChild("##matrix_right", ImVec2(0.0f, avail.y), false,
                          ImGuiWindowFlags_NoScrollbar)) {
        theme::section_divider("VIEW MATRIX");
        ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
        draw_matrix_grid(snap.view.data(), snap.camera_ready);

        ImGui::Dummy(ImVec2(0.0f, theme::space_md));
        theme::section_divider("CAMERA");
        ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
        if (snap.camera_ready &&
            ImGui::BeginTable("##matrix_camera", 2,
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            draw_vec_row("pos", snap.cam_pos, true);
            draw_vec_row("forward", snap.cam_forward, false);
            draw_vec_row("up", snap.cam_up, false);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("fov_y");
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f deg", snap.fov_y);
            ImGui::EndTable();
        } else if (!snap.camera_ready) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("unavailable");
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, theme::space_md));
        theme::section_divider("UNITS");
        ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
        ImGui::Text("%d alive", static_cast<int>(snap.units.size()));
    }
    ImGui::EndChild();
}

}  // namespace dxs
