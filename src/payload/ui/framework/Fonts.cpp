#include "Fonts.hpp"

#include "Icons.hpp"
#include "core/Logger.hpp"

#include <Windows.h>
#include <imgui.h>

#include <filesystem>

namespace dxs::fonts {

namespace {

std::filesystem::path windir_font(const wchar_t* name) {
    wchar_t buf[MAX_PATH]{};
    GetWindowsDirectoryW(buf, MAX_PATH);
    return std::filesystem::path(buf) / L"Fonts" / name;
}

const ImWchar* ui_base_ranges() {
    static const ImWchar r[] = {
        0x0020, 0x00FF,
        0x00B7, 0x00B7,   // middle dot
        0x2013, 0x2014,   // em/en dashes
        0x2018, 0x201F,   // smart quotes
        0x2022, 0x2022,   // bullet
        0x2026, 0x2026,   // ellipsis
        0x2190, 0x21FF,   // arrows
        0x25A0, 0x25FF,   // geometric
        0x2713, 0x2716,   // check/cross
        0,
    };
    return r;
}

}  // namespace

void load() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontGlobalScale    = 1.0f;
    io.ConfigFlags       |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsResizeFromEdges = true;

    const auto segoe = windir_font(L"segoeui.ttf");
    if (!std::filesystem::exists(segoe)) {
        DXS_WARN("Fonts: segoeui.ttf missing, using ImGui default");
        io.Fonts->AddFontDefault();
        return;
    }

    // Latin UI base: Segoe UI 15 px at 2x/2x oversampling. Windows UI font.
    ImFontConfig cfg_base;
    cfg_base.OversampleH        = 2;
    cfg_base.OversampleV        = 2;
    cfg_base.PixelSnapH         = false;
    cfg_base.RasterizerMultiply = 1.00f;     // no over-boosting — stays crisp
    cfg_base.GlyphExtraSpacing  = ImVec2(0.10f, 0);

    io.Fonts->AddFontFromFileTTF(
        segoe.string().c_str(), 15.0f, &cfg_base, ui_base_ranges());

    // CJK: Microsoft YaHei UI Light (msyhl) if present (cleanest at body size),
    // fall back to Regular. Full "ChineseSimplifiedCommon" range — 2500 glyphs
    // covering every character used in everyday mainland tech prose.
    const auto yaheil  = windir_font(L"msyhl.ttc");
    const auto yahei   = windir_font(L"msyh.ttc");
    const auto& cjk_file =
        std::filesystem::exists(yaheil) ? yaheil :
        std::filesystem::exists(yahei)  ? yahei  : std::filesystem::path();

    if (!cjk_file.empty()) {
        ImFontConfig cfg_cjk;
        cfg_cjk.MergeMode           = true;
        cfg_cjk.OversampleH         = 2;
        cfg_cjk.OversampleV         = 1;
        cfg_cjk.PixelSnapH          = true;     // CJK reads best pixel-aligned
        cfg_cjk.RasterizerMultiply  = 1.00f;
        cfg_cjk.GlyphOffset         = ImVec2(0, 1);   // baseline nudge for visual harmony w/ Segoe
        io.Fonts->AddFontFromFileTTF(
            cjk_file.string().c_str(),
            16.5f,                                 // slightly larger — CJK bodies read better at 16+
            &cfg_cjk,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    } else {
        DXS_WARN("Fonts: no MS YaHei variant installed; Chinese will be blank");
    }

    // Windows 11 Fluent icons.
    const auto fluent = windir_font(L"SegoeIcons.ttf");
    if (std::filesystem::exists(fluent)) {
        ImFontConfig cfg_icon = cfg_base;
        cfg_icon.MergeMode       = true;
        cfg_icon.GlyphMinAdvanceX = 15.0f;
        cfg_icon.GlyphOffset     = ImVec2(0, 1);
        cfg_icon.OversampleH     = 1;
        cfg_icon.OversampleV     = 1;
        io.Fonts->AddFontFromFileTTF(
            fluent.string().c_str(), 15.0f, &cfg_icon, dxs::icons::range());
    }

    io.Fonts->Build();
    DXS_INFO("Fonts loaded — atlas {} x {}  ({})",
             io.Fonts->TexWidth, io.Fonts->TexHeight,
             cjk_file.filename().string());
}

}  // namespace dxs::fonts
