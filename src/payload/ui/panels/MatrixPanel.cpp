#include "MatrixPanel.hpp"

#include "core/Localization.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <cmath>

namespace dxs {

namespace {

void draw_matrix(const char* title, const float m[16], bool have) {
    const ImVec2 box_tl = ImGui::GetCursorScreenPos();
    const float  box_h  = 166.0f;
    const ImVec2 box_br = box_tl + ImVec2(ImGui::GetContentRegionAvail().x, box_h);
    theme::draw_surface(ImGui::GetWindowDrawList(), box_tl, box_br,
                        theme::radius_lg, theme::bg_surface, &theme::info, 2.0f, false);
    ImGui::SetCursorScreenPos(box_tl + ImVec2(theme::card_pad_x, theme::card_pad_y));
    theme::section_label(title);
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    if (!have) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted("  unavailable");
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(box_tl.x, box_br.y + theme::space_md));
        return;
    }

    if (ImGui::BeginTable(title, 4,
                          ImGuiTableFlags_BordersInner |
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchSame)) {
        for (int r = 0; r < 4; ++r) {
            ImGui::TableNextRow();
            for (int c = 0; c < 4; ++c) {
                ImGui::TableSetColumnIndex(c);
                const float v = m[r * 4 + c];
                ImVec4 col = theme::text_primary;
                if (std::fabs(v) < 1e-6f)        col = theme::text_faded;
                else if (std::fabs(v - 1.0f) < 1e-6f) col = theme::good;
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::Text("%+9.4f", v);
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
    ImGui::SetCursorScreenPos(ImVec2(box_tl.x, box_br.y + theme::space_md));
}

}  // namespace

void MatrixPanel::draw() {
    auto snap = CameraSampler::instance().snapshot();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    theme::section_label("CAMERA SAMPLER");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("matrix.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, theme::space_md));

    // Status strip — tells the user whether the sampler is hot and how recent
    // the matrix values are. A dead camera usually means "not in a 3D scene".
    const double age = snap.sample_time > 0
        ? CameraSampler::now() - snap.sample_time
        : -1.0;

    const ImVec2 strip_tl = ImGui::GetCursorScreenPos();
    const ImVec2 strip_br = strip_tl + ImVec2(ImGui::GetContentRegionAvail().x, 52.0f);
    theme::draw_surface(dl, strip_tl, strip_br, theme::radius_lg, theme::bg_surface,
                        snap.camera_ready ? &theme::good : &theme::warn, 3.0f, false);
    ImGui::SetCursorScreenPos(strip_tl + ImVec2(theme::card_pad_x, theme::space_sm));
    theme::status_chip(snap.camera_ready ? theme::Status::Good : theme::Status::Warn,
                       snap.camera_ready ? "Camera ready" : "Camera not bound");
    ImGui::SameLine(0, theme::space_sm);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    if (age >= 0) {
        ImGui::Text("sample %d  ·  age %.2fs  ·  %.1f ms",
                    snap.sample_count, age, snap.last_duration * 1000.0);
    } else {
        ImGui::TextUnformatted("waiting for first sample");
    }
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(strip_tl.x, strip_br.y + theme::space_md));

    // Sampler rate control — directly feeds back into CameraSampler.
    theme::section_label("SAMPLING");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    float hz = CameraSampler::instance().rate_hz();
    ImGui::SetNextItemWidth(140);
    if (ImGui::SliderFloat("sample rate (Hz)##matrix", &hz, 1.0f, 60.0f, "%.0f")) {
        CameraSampler::instance().set_rate_hz(hz);
    }

    ImGui::Dummy(ImVec2(0, theme::space_md));

    draw_matrix("VIEW",       snap.view.data(), snap.camera_ready);
    draw_matrix("PROJECTION", snap.proj.data(), snap.camera_ready);

    if (snap.camera_ready) {
        theme::section_label("POSE");
        ImGui::Dummy(ImVec2(0, theme::space_xs));

        // Flat 2-column table: label on the left at a fixed 130 px column,
        // values stretch right. Far better than leading-space alignment
        // (Inter is proportional — spaces aren't equal-width, so every
        // "camera pos   = " etc. lands at a different X and the numbers
        // never line up visually).
        if (ImGui::BeginTable("##pose", 2,
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 130.0f);
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
            row("camera pos",  "(%+.2f, %+.2f, %+.2f)",
                snap.cam_pos.x, snap.cam_pos.y, snap.cam_pos.z);
            row("forward",     "(%+.3f, %+.3f, %+.3f)",
                snap.cam_forward.x, snap.cam_forward.y, snap.cam_forward.z);
            row("fov (y, deg)","%.2f", snap.fov_y);
            if (snap.player_ready) {
                row("local player uid", "%llu",
                    static_cast<unsigned long long>(snap.player_uid));
                row("local player pos", "(%+.2f, %+.2f, %+.2f)",
                    snap.player_pos.x, snap.player_pos.y, snap.player_pos.z);
            }
            ImGui::EndTable();
        }
    }

    if (!snap.last_error.empty()) {
        ImGui::Dummy(ImVec2(0, theme::space_sm));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::TextWrapped("last sample note: %s", snap.last_error.c_str());
        ImGui::PopStyleColor();
    }
}

}  // namespace dxs
