#include "MatrixPanel.hpp"

#include "core/Localization.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <cmath>
#include <sstream>

namespace dxs {

namespace {

// Best-effort matrix probe. Tries three historically common NeoX3 accessor
// paths before giving up — none of them are guaranteed to exist on this
// specific game build, but as a dev tool running in your own game, you can
// swap these for whatever your engine actually exposes.
constexpr const char* k_probe = R"PY(
def _dxs_mat_print(name, m):
    try:
        flat = [float(v) for row in m for v in row]
    except Exception:
        try: flat = [float(v) for v in m]
        except Exception: return
    if len(flat) != 16: return
    print(f"DXS_MAT {name} " + " ".join(f"{v:.6f}" for v in flat))

def _dxs_probe():
    try:
        import BigWorld as bw
        cam = bw.camera()
        _dxs_mat_print("view", cam.view)
        _dxs_mat_print("proj", cam.projection)
        return True
    except Exception: pass
    try:
        import neox.camera as nc
        c = nc.get_active()
        _dxs_mat_print("view", c.view_matrix())
        _dxs_mat_print("proj", c.projection_matrix())
        return True
    except Exception: pass
    try:
        from engine import camera
        c = camera.current()
        _dxs_mat_print("view", c.view)
        _dxs_mat_print("proj", c.proj)
        return True
    except Exception: pass
    return False

if not _dxs_probe():
    print("DXS_MAT_NONE")
)PY";

bool parse_matrix_line(const std::string& line, const char* name, float out[16]) {
    std::istringstream is(line);
    std::string tok; is >> tok;                                   // "DXS_MAT"
    if (tok != "DXS_MAT") return false;
    is >> tok;
    if (tok != name) return false;
    for (int i = 0; i < 16; ++i) {
        if (!(is >> out[i])) return false;
    }
    return true;
}

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
                if (std::fabs(v) < 1e-6f) col = theme::text_faded;
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

void MatrixPanel::draw_matrix_box(const char* t, const float m[16], bool h) {
    draw_matrix(t, m, h);
}

void MatrixPanel::kick_python_probe() {
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) return;
    last_kick_at_ = ImGui::GetTime();
    bridge.exec(k_probe);
}

void MatrixPanel::draw() {
    auto& bridge = PythonBridge::instance();

    // Drain the Python output every frame once a probe is in-flight.
    if (bridge.ready()) {
        std::string chunk = bridge.drain_output();
        std::istringstream is(chunk);
        std::string line;
        while (std::getline(is, line)) {
            if (parse_matrix_line(line, "view", view_)) have_view_ = true;
            if (parse_matrix_line(line, "proj", proj_)) have_proj_ = true;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "View / projection matrix inspector. Probes a handful of common NeoX3 "
        "accessors through the in-process Python bridge. When the subsystem "
        "survey lands, the DX11 VS-constant-buffer snoop will be the primary "
        "source; Python is the fallback / validation path.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));

    ImGui::BeginDisabled(!bridge.ready());
    if (ImGui::Button(L("common.refresh").data(), ImVec2(110, 28))) kick_python_probe();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    if (last_kick_at_ > 0.0) {
        ImGui::Text("probed %.1fs ago", ImGui::GetTime() - last_kick_at_);
    } else {
        ImGui::TextUnformatted("not yet probed");
    }
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    draw_matrix("VIEW",       view_, have_view_);
    draw_matrix("PROJECTION", proj_, have_proj_);

    if (!have_view_ && !have_proj_) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::TextWrapped(
            "No matrix source found yet. The probe tried BigWorld.camera(), "
            "neox.camera.get_active(), and engine.camera.current() — none "
            "exist on this build. The subsystem survey (Codex task in flight) "
            "will give us the C++ camera singleton address for a direct read.");
        ImGui::PopStyleColor();
    }
}

}  // namespace dxs
