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
    // DengXian Light (Dengl.ttf) — Microsoft's modern Chinese UI font; lighter
    // and cleaner than the default YaHei. Full ChineseSimplifiedCommon range.
    const auto deng_l = windir_font(L"Dengl.ttf");
    const auto deng   = windir_font(L"Deng.ttf");
    const auto& cjk = std::filesystem::exists(deng_l) ? deng_l :
                      std::filesystem::exists(deng)   ? deng   : std::filesystem::path();
    if (!cjk.empty()) {
        ImFontConfig cfg_cjk;
        cfg_cjk.MergeMode           = true;
        cfg_cjk.OversampleH         = 2;
        cfg_cjk.OversampleV         = 1;
        cfg_cjk.PixelSnapH          = true;
        cfg_cjk.RasterizerMultiply  = 1.00f;
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

    io.Fonts->Build();
    DXS_INFO("Fonts: {} + {} + {} ({} x {} atlas)",
             base_path.filename().string(),
             cjk.empty() ? "-" : cjk.filename().string(),
             std::filesystem::exists(fluent) ? "SegoeIcons" : "-",
             io.Fonts->TexWidth, io.Fonts->TexHeight);
}

}  // namespace dxs::fonts
