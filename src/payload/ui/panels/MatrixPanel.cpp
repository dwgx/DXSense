#include "MatrixPanel.hpp"

#include "core/Localization.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <cmath>

namespace dxs {

namespace {

void draw_matrix(const char* title, const float m[16], bool have) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (!have) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted("  unavailable");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));
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
    ImGui::Dummy(ImVec2(0, 6));
}

}  // namespace

void MatrixPanel::draw() {
    auto snap = CameraSampler::instance().snapshot();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("matrix.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 6));

    // Status strip — tells the user whether the sampler is hot and how recent
    // the matrix values are. A dead camera usually means "not in a 3D scene".
    const double age = snap.sample_time > 0
        ? CameraSampler::now() - snap.sample_time
        : -1.0;

    if (snap.camera_ready) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::good);
        ImGui::Text("camera ready");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::Text("camera not bound (not in a 3D scene yet)");
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    if (age >= 0) {
        ImGui::Text("  ·  sample %d  ·  age %.2fs  ·  %.1f ms",
                    snap.sample_count, age, snap.last_duration * 1000.0);
    } else {
        ImGui::TextUnformatted("  ·  waiting for first sample");
    }
    ImGui::PopStyleColor();

    // Sampler rate control — directly feeds back into CameraSampler.
    float hz = CameraSampler::instance().rate_hz();
    ImGui::SetNextItemWidth(140);
    if (ImGui::SliderFloat("sample rate (Hz)##matrix", &hz, 1.0f, 60.0f, "%.0f")) {
        CameraSampler::instance().set_rate_hz(hz);
    }

    ImGui::Dummy(ImVec2(0, 10));

    draw_matrix("VIEW",       snap.view.data(), snap.camera_ready);
    draw_matrix("PROJECTION", snap.proj.data(), snap.camera_ready);

    if (snap.camera_ready) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::Text("camera pos   = (%+.2f, %+.2f, %+.2f)",
                    snap.cam_pos.x, snap.cam_pos.y, snap.cam_pos.z);
        ImGui::Text("forward      = (%+.3f, %+.3f, %+.3f)",
                    snap.cam_forward.x, snap.cam_forward.y, snap.cam_forward.z);
        ImGui::Text("fov (y, deg) = %.2f", snap.fov_y);
        if (snap.player_ready) {
            ImGui::Text("local player = uid %llu @ (%+.2f, %+.2f, %+.2f)",
                        static_cast<unsigned long long>(snap.player_uid),
                        snap.player_pos.x, snap.player_pos.y, snap.player_pos.z);
        }
        ImGui::PopStyleColor();
    }

    if (!snap.last_error.empty()) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::TextWrapped("last sample note: %s", snap.last_error.c_str());
        ImGui::PopStyleColor();
    }
}

}  // namespace dxs
