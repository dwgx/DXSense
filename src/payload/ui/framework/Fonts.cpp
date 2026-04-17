#include "Fonts.hpp"

#include "Icons.hpp"
#include "core/Logger.hpp"

#include <Windows.h>
#include <imgui.h>

#include <filesystem>

namespace dxs::fonts {

namespace {

// We read fonts from %WINDIR%\Fonts. Both of these have been shipped on
// every consumer Windows install since Windows 7, so we don't bother with
// embedded TTF fallbacks.
std::filesystem::path windir_font(const wchar_t* name) {
    wchar_t buf[MAX_PATH]{};
    GetWindowsDirectoryW(buf, MAX_PATH);
    return std::filesystem::path(buf) / L"Fonts" / name;
}

// Curated glyph ranges: ASCII + Latin-1 Supplement + common UI symbols.
// This lets us render bullets, toggles, arrows without pulling the whole
// Basic Multilingual Plane from Segoe.
const ImWchar* ui_symbol_ranges() {
    static const ImWchar r[] = {
        0x0020, 0x00FF,   // Basic Latin + Latin-1 Supplement
        0x2013, 0x2014,   // en/em dashes
        0x2018, 0x201F,   // smart quotes
        0x2022, 0x2022,   // •
        0x2026, 0x2026,   // …
        0x2190, 0x21FF,   // arrows
        0x25A0, 0x25FF,   // geometric shapes (●, ○, ▶, ■, ...)
        0x2713, 0x2716,   // check / cross
        0x03B1, 0x03C9,   // greek lowercase (α β γ for physics labels)
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

    // ---- Primary Latin / UI font (Segoe UI 15 px semibold-ish weight) --------
    const auto segoe = windir_font(L"segoeui.ttf");
    if (!std::filesystem::exists(segoe)) {
        DXS_WARN("Fonts: segoeui.ttf missing, using ImGui default bitmap font");
        io.Fonts->AddFontDefault();
        return;
    }

    ImFontConfig cfg_base;
    cfg_base.OversampleH    = 3;
    cfg_base.OversampleV    = 2;
    cfg_base.PixelSnapH     = false;
    cfg_base.RasterizerMultiply = 1.05f;
    cfg_base.GlyphExtraSpacing  = ImVec2(0.25f, 0);

    ImFont* main = io.Fonts->AddFontFromFileTTF(
        segoe.string().c_str(),
        15.0f,
        &cfg_base,
        ui_symbol_ranges());
    (void)main;

    // ---- CJK fallback (merged so a single ImFont covers every glyph) ---------
    const auto yahei = windir_font(L"msyh.ttc");
    if (std::filesystem::exists(yahei)) {
        ImFontConfig cfg_cjk = cfg_base;
        cfg_cjk.MergeMode = true;
        cfg_cjk.FontNo    = 0;                    // first face in the .ttc
        cfg_cjk.OversampleH = 2;
        cfg_cjk.OversampleV = 1;                  // CJK doesn't benefit from V over-sampling
        cfg_cjk.RasterizerMultiply = 1.00f;
        // GetGlyphRangesChineseFull instead of the narrower "common" set:
        // the 2500-common list misses plenty of normal CJK characters that
        // show up in ordinary technical prose (辑 U+8F91, 窗 U+7A97, ...).
        // The atlas grows but we accept the cost — correctness wins.
        io.Fonts->AddFontFromFileTTF(
            yahei.string().c_str(),
            16.0f,
            &cfg_cjk,
            io.Fonts->GetGlyphRangesChineseFull());
    } else {
        DXS_WARN("Fonts: msyh.ttc missing; CJK glyphs will render as ?");
    }

    // ---- Windows 11 Fluent icons (merged) -----------------------------------
    // SegoeIcons.ttf ships with every Win10+ install and matches the
    // modern-Windows / Claude-site aesthetic (clean monoweight line icons).
    const auto fluent = windir_font(L"SegoeIcons.ttf");
    if (std::filesystem::exists(fluent)) {
        ImFontConfig cfg_icon = cfg_base;
        cfg_icon.MergeMode       = true;
        cfg_icon.GlyphMinAdvanceX = 15.0f;        // keep icons a uniform width
        cfg_icon.GlyphOffset     = ImVec2(0, 1);  // nudge down 1 px for baseline alignment
        cfg_icon.OversampleH     = 2;
        cfg_icon.OversampleV     = 1;
        io.Fonts->AddFontFromFileTTF(
            fluent.string().c_str(),
            15.0f,
            &cfg_icon,
            dxs::icons::range());
    } else {
        DXS_WARN("Fonts: SegoeIcons.ttf missing; icons will render as ?");
    }

    io.Fonts->Build();
    DXS_INFO("Fonts loaded — atlas {} x {}",
             io.Fonts->TexWidth, io.Fonts->TexHeight);
}

}  // namespace dxs::fonts
