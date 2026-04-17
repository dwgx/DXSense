#include "MatrixPanel.hpp"

#include "core/Localization.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <cmath>
#include <sstream>

namespace dxs {

namespace {

// NeoX3 on dwrg exposes its camera system under `core.camera`. A Python REPL
// dump earlier revealed ~20 CamMode subclasses + a `CameraCtrl` controller
// and a `PhantomCamMgr` manager — we try the controller + manager first,
// then fall back to any BigWorld-style global or a generic gc scan.
constexpr const char* k_probe = R"PY(
def _dxs_flat(m):
    try:
        if hasattr(m, '__len__') and len(m) == 16:
            return [float(v) for v in m]
        if hasattr(m, '__len__') and len(m) == 4:
            out = []
            for r in m:
                if hasattr(r, '__len__') and len(r) == 4:
                    out.extend(float(v) for v in r)
            if len(out) == 16: return out
        if hasattr(m, 'toList'):
            v = m.toList()
            return [float(x) for x in v][:16]
    except Exception: pass
    return None

def _dxs_mat_print(name, m):
    f = _dxs_flat(m)
    if not f: return False
    print("DXS_MAT", name, " ".join(f"{v:.6f}" for v in f))
    return True

def _dxs_probe():
    # core.camera.CameraCtrl — the controller singleton in NeoX3.
    try:
        import core.camera as cc
        ctrl = cc.CameraCtrl
        if hasattr(ctrl, 'instance'): ctrl = ctrl.instance()
        v = None; p = None
        for vname in ('view', 'view_matrix', 'getViewMatrix', 'get_view'):
            if hasattr(ctrl, vname):
                a = getattr(ctrl, vname); v = a() if callable(a) else a; break
        for pname in ('projection', 'proj', 'projection_matrix', 'getProjection'):
            if hasattr(ctrl, pname):
                a = getattr(ctrl, pname); p = a() if callable(a) else a; break
        ok = False
        if v is not None and _dxs_mat_print("view", v): ok = True
        if p is not None and _dxs_mat_print("proj", p): ok = True
        if ok: return True
    except Exception as e: print("probe core.camera:", e)

    # PhantomCamMgr — the active camera manager (often holds the current cam).
    try:
        from common_cs.mgr import PhantomCamMgr as pcm
        m = pcm.get_instance() if hasattr(pcm, 'get_instance') else pcm()
        cam = m.GetCurrentCamera() if hasattr(m, 'GetCurrentCamera') else None
        if cam is not None:
            if _dxs_mat_print("view", getattr(cam, 'view', cam.GetViewMatrix())):
                if hasattr(cam, 'projection'):
                    _dxs_mat_print("proj", cam.projection)
                return True
    except Exception as e: print("probe PhantomCamMgr:", e)

    # Generic scan — any global object that looks like a camera.
    try:
        import gc
        for o in gc.get_objects():
            if type(o).__name__ in ('Camera', 'GameCamera', 'CameraActor'):
                v = getattr(o, 'view', None)
                p = getattr(o, 'projection', None) or getattr(o, 'proj', None)
                if v is not None and _dxs_mat_print("view", v):
                    if p is not None: _dxs_mat_print("proj", p)
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
    ImGui::TextWrapped("%s", L("matrix.intro").data());
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
        ImGui::TextWrapped("%s", L("matrix.no_source").data());
        ImGui::PopStyleColor();
    }
}

}  // namespace dxs
