#include "Overlay.hpp"

#include "core/Logger.hpp"

#include <imgui.h>

namespace dxs {

namespace {
constexpr int kToggleKey = VK_INSERT;  // Cheap default; Overlay owns policy, not WndProcHook.
}

Overlay& Overlay::instance() {
    static Overlay o;
    return o;
}

void Overlay::configure_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.GrabRounding      = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.WindowPadding     = ImVec2(10, 8);
    s.FramePadding      = ImVec2(8,  4);
    s.ItemSpacing       = ImVec2(8,  6);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.07f, 0.08f, 0.10f, 0.92f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.18f, 0.40f, 0.70f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.26f, 0.55f, 0.90f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.15f, 0.30f, 0.55f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.40f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_Text]            = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    c[ImGuiCol_Border]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_Separator]       = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
}

void Overlay::draw() {
    ++frame_;
    if (!visible_) return;

    ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("DXSense / Debug Runtime", nullptr,
                     ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextDisabled("dwrg / NeoX3 overlay — foundation build");
        ImGui::Separator();

        ImGui::Text("frame: %d", frame_);
        ImGui::Text("io.Framerate: %.1f fps", ImGui::GetIO().Framerate);
        ImGui::Spacing();

        ImGui::Checkbox("ImGui demo window", &show_demo_);
        ImGui::SameLine();
        ImGui::TextDisabled("(INSERT to toggle overlay)");

        ImGui::Spacing();
        ImGui::TextWrapped(
            "This is the injection foundation. Subsystems online: Logger, "
            "HookManager (MinHook), DX11 backend (vtable-scanned Present/ResizeBuffers), "
            "WndProc subclass, ImGui DX11 + Win32 backends.");
    }
    ImGui::End();

    if (show_demo_) ImGui::ShowDemoWindow(&show_demo_);
}

void Overlay::route_input(UINT msg, WPARAM w, LPARAM /*l*/) {
    if (msg == WM_KEYDOWN && static_cast<int>(w) == kToggleKey) {
        visible_ = !visible_;
        DXS_INFO("overlay visibility: {}", visible_);
    }
}

}  // namespace dxs
