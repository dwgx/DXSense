#include "Fonts.hpp"

#include "Icons.hpp"
#include "MdiIcons.hpp"
#include "core/Logger.hpp"

#include <Windows.h>
#include <imgui.h>

#include <filesystem>

namespace dxs::fonts {

namespace {
ImFont* g_splash_title = nullptr;
}

ImFont* splash_title() { return g_splash_title; }

namespace {

std::filesystem::path windir_font(const wchar_t* name) {
    wchar_t buf[MAX_PATH]{};
    GetWindowsDirectoryW(buf, MAX_PATH);
    return std::filesystem::path(buf) / L"Fonts" / name;
}

std::filesystem::path dll_dir() {
    wchar_t buf[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&dll_dir), &self);
    GetModuleFileNameW(self, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}

const ImWchar* ui_base_ranges() {
    static const ImWchar r[] = {
        0x0020, 0x00FF,
        0x00B7, 0x00B7,
        0x2013, 0x2014,
        0x2018, 0x201F,
        0x2022, 0x2022,
        0x2026, 0x2026,
        0x2190, 0x21FF,
        0x25A0, 0x25FF,
        0x2713, 0x2716,
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

    // --- Latin / UI base ------------------------------------------------------
    // Prefer Inter (shipped next to DXSense.dll under fonts/), fall back to
    // Segoe UI as an absolute last resort. Inter is the de facto modern
    // UI sans — closest fit to the "DejaVu-like" aesthetic the user asked for
    // while being sharper at body size than DejaVu Sans.
    const auto inter = dll_dir() / "fonts" / "Inter.ttf";
    const auto segoe = windir_font(L"segoeui.ttf");

    std::filesystem::path base_path;
    if      (std::filesystem::exists(inter)) base_path = inter;
    else if (std::filesystem::exists(segoe)) base_path = segoe;
    else {
        DXS_WARN("Fonts: no UI base font available");
        io.Fonts->AddFontDefault();
        return;
    }

    ImFontConfig cfg_base;
    cfg_base.OversampleH        = 3;
    cfg_base.OversampleV        = 2;
    cfg_base.PixelSnapH         = false;
    cfg_base.RasterizerMultiply = 1.00f;
    cfg_base.GlyphExtraSpacing  = ImVec2(0.10f, 0);
    io.Fonts->AddFontFromFileTTF(
        base_path.string().c_str(), 15.0f, &cfg_base, ui_base_ranges());

    // --- CJK ------------------------------------------------------------------
    // Microsoft YaHei (msyh.ttc/ttf) — Standard Windows Chinese UI font.
    // Extremely readable at 16px. Replaces DengXian which renders terribly jagged
    // on un-scaled DX11 contexts.
    const auto msyh_tc  = windir_font(L"msyh.ttc");
    const auto msyh_ttf = windir_font(L"msyh.ttf");
    const auto& cjk = std::filesystem::exists(msyh_tc)  ? msyh_tc :
                      std::filesystem::exists(msyh_ttf) ? msyh_ttf : std::filesystem::path();
    if (!cjk.empty()) {
        ImFontConfig cfg_cjk;
        cfg_cjk.MergeMode           = true;
        cfg_cjk.OversampleH         = 2; // Force 2x scaling for CJK anti-aliasing
        cfg_cjk.OversampleV         = 2;
        cfg_cjk.PixelSnapH          = false; // Disabled to smoothly position on float boundaries
        cfg_cjk.RasterizerMultiply  = 1.10f; // Slight bump for pure white readability
        cfg_cjk.GlyphOffset         = ImVec2(0, 1);
        // ChineseFull covers all ~21K CJK Unified Ideographs. We pay a
        // larger atlas (~4K x 4K) in exchange for never rendering tofu on
        // any Simplified or Traditional character the game or localization
        // strings may contain. The ChineseSimplifiedCommon range used to
        // be here, but it dropped characters like 擎 / 次 / 帧 that appear
        // in our own UI strings.
        io.Fonts->AddFontFromFileTTF(
            cjk.string().c_str(), 16.0f, &cfg_cjk,
            io.Fonts->GetGlyphRangesChineseFull());
    }

    // --- Icons ----------------------------------------------------------------
    const auto fluent = windir_font(L"SegoeIcons.ttf");
    if (std::filesystem::exists(fluent)) {
        ImFontConfig cfg_icon;
        cfg_icon.MergeMode         = true;
        cfg_icon.OversampleH       = 1;
        cfg_icon.OversampleV       = 1;
        cfg_icon.GlyphMinAdvanceX  = 15.0f;
        cfg_icon.GlyphOffset       = ImVec2(0, 1);
        io.Fonts->AddFontFromFileTTF(
            fluent.string().c_str(), 15.0f, &cfg_icon, dxs::icons::range());
    }

    const auto mdi = dll_dir() / "fonts" / "MaterialDesignIcons.ttf";
    if (std::filesystem::exists(mdi)) {
        ImFontConfig cfg_mdi;
        cfg_mdi.MergeMode         = true;
        cfg_mdi.OversampleH       = 1;
        cfg_mdi.OversampleV       = 1;
        cfg_mdi.GlyphMinAdvanceX  = 15.0f;
        cfg_mdi.GlyphOffset       = ImVec2(0, 1);
        io.Fonts->AddFontFromFileTTF(
            mdi.string().c_str(), 15.0f, &cfg_mdi, dxs::mdi::range());
    }

    // --- Splash hero font -----------------------------------------------------
    // Rasterise the splash title at 96 px directly instead of upscaling the
    // 15-px UI font 5×. Upscaling a 15-px glyph atlas by 5× is bilinear —
    // edges look chewy. Native 96 px keeps every stroke razor-sharp.
    // Latin + a tiny glyph set (DXSENSE, BYE, THX · FOR USING) is all we
    // need here, so the atlas cost is minor (< 200 KB).
    {
        static const ImWchar splash_range[] = {
            0x0020, 0x007E,   // basic Latin + space + ASCII punctuation
            0x00B7, 0x00B7,   // middle dot (·)
            0,
        };
        ImFontConfig cfg_splash;
        cfg_splash.OversampleH        = 3;
        cfg_splash.OversampleV        = 2;
        cfg_splash.PixelSnapH         = true;
        cfg_splash.RasterizerMultiply = 1.00f;
        g_splash_title = io.Fonts->AddFontFromFileTTF(
            base_path.string().c_str(), 96.0f, &cfg_splash, splash_range);
    }

    io.Fonts->Build();
    DXS_INFO("Fonts: {} + {} + {} + {} + splash96 ({} x {} atlas)",
             base_path.filename().string(),
             cjk.empty() ? "-" : cjk.filename().string(),
             std::filesystem::exists(fluent) ? "SegoeIcons" : "-",
             std::filesystem::exists(mdi) ? mdi.filename().string() : "-",
             io.Fonts->TexWidth, io.Fonts->TexHeight);
}

}  // namespace dxs::fonts
