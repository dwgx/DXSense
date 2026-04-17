#include "Theme.hpp"

namespace dxs::theme {

void apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding       = corner_md;
    s.ChildRounding        = corner_sm;
    s.FrameRounding        = corner_sm;
    s.PopupRounding        = corner_md;
    s.GrabRounding         = corner_sm;
    s.ScrollbarRounding    = corner_sm;
    s.TabRounding          = corner_sm;

    s.WindowBorderSize     = 0.0f;
    s.ChildBorderSize      = 0.0f;
    s.FrameBorderSize      = 0.0f;
    s.PopupBorderSize      = 1.0f;

    s.WindowPadding        = {space_4, space_3};
    s.FramePadding         = {space_3,  6};
    s.CellPadding          = {space_3,  6};
    s.ItemSpacing          = {space_3,  8};
    s.ItemInnerSpacing     = {space_2,  4};
    s.IndentSpacing        = 20.0f;
    s.ScrollbarSize        = 10.0f;
    s.GrabMinSize          = 10.0f;

    s.WindowTitleAlign     = {0.0f, 0.5f};
    s.ButtonTextAlign      = {0.5f, 0.5f};
    s.SelectableTextAlign  = {0.0f, 0.5f};

    // Higher-quality anti-aliasing: circle tessellation tolerance down a bit,
    // curves segment-max lifted so big rounded panels don't show faceting.
    // Both AntiAliasedLines and AntiAliasedFill are on by default, but we
    // set explicitly for clarity — AntiAliasedLinesUseTex needs the font
    // atlas to be built with ImFontAtlasFlags_NoBakedLines UNSET (it is by
    // default), so lines get baked texture AA instead of per-frame geometry.
    s.AntiAliasedLines         = true;
    s.AntiAliasedLinesUseTex   = true;
    s.AntiAliasedFill          = true;
    s.CurveTessellationTol     = 1.0f;
    s.CircleTessellationMaxError = 0.20f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = text_primary;
    c[ImGuiCol_TextDisabled]          = text_faded;
    c[ImGuiCol_WindowBg]              = bg_root;
    c[ImGuiCol_ChildBg]               = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg]               = bg_elevated;
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = {0, 0, 0, 0};

    c[ImGuiCol_FrameBg]               = bg_surface;
    c[ImGuiCol_FrameBgHovered]        = bg_hover;
    c[ImGuiCol_FrameBgActive]         = bg_active;

    c[ImGuiCol_TitleBg]               = bg_surface;
    c[ImGuiCol_TitleBgActive]         = bg_elevated;
    c[ImGuiCol_TitleBgCollapsed]      = bg_surface;

    c[ImGuiCol_MenuBarBg]             = bg_surface;

    c[ImGuiCol_ScrollbarBg]           = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab]         = bg_hover;
    c[ImGuiCol_ScrollbarGrabHovered]  = bg_active;
    c[ImGuiCol_ScrollbarGrabActive]   = accent_edge;

    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accent_hot;

    c[ImGuiCol_Button]                = bg_elevated;
    c[ImGuiCol_ButtonHovered]         = bg_hover;
    c[ImGuiCol_ButtonActive]          = accent_soft;

    c[ImGuiCol_Header]                = accent_soft;
    c[ImGuiCol_HeaderHovered]         = bg_hover;
    c[ImGuiCol_HeaderActive]          = accent_edge;

    c[ImGuiCol_Separator]             = divider;
    c[ImGuiCol_SeparatorHovered]      = accent_edge;
    c[ImGuiCol_SeparatorActive]       = accent;

    c[ImGuiCol_ResizeGrip]            = {0, 0, 0, 0};
    c[ImGuiCol_ResizeGripHovered]     = accent_edge;
    c[ImGuiCol_ResizeGripActive]      = accent;

    c[ImGuiCol_Tab]                   = bg_surface;
    c[ImGuiCol_TabHovered]            = bg_hover;
    c[ImGuiCol_TabSelected]           = accent_soft;
    c[ImGuiCol_TabDimmed]             = bg_surface;
    c[ImGuiCol_TabDimmedSelected]     = bg_surface;

    c[ImGuiCol_PlotLines]             = info;
    c[ImGuiCol_PlotLinesHovered]      = accent;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accent_edge;

    c[ImGuiCol_TableHeaderBg]         = bg_elevated;
    c[ImGuiCol_TableBorderStrong]     = divider;
    c[ImGuiCol_TableBorderLight]      = divider;
    c[ImGuiCol_TableRowBg]            = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]         = {1, 1, 1, 0.014f};

    c[ImGuiCol_TextSelectedBg]        = accent_soft;
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_NavCursor]             = accent;
    c[ImGuiCol_NavWindowingHighlight] = accent_edge;
    c[ImGuiCol_NavWindowingDimBg]     = {0, 0, 0, 0.35f};
    c[ImGuiCol_ModalWindowDimBg]      = {0, 0, 0, 0.55f};
}

void draw_shadow(ImVec2 tl, ImVec2 br, float rounding, float extent) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Three concentric softened rectangles for a cheap approximation of a
    // Gaussian shadow. Darkness and radius decrease as we move outward.
    struct Layer { float offset; float alpha; };
    constexpr Layer layers[] = {
        {0.28f, 0.45f}, {0.55f, 0.22f}, {1.00f, 0.08f},
    };
    for (auto& l : layers) {
        ImVec4 col = shadow;
        col.w = l.alpha * 0.35f;
        const float grow = extent * l.offset;
        dl->AddRectFilled(tl - ImVec2(grow, grow - 2),
                          br + ImVec2(grow, grow + 2),
                          to_u32(col),
                          rounding + grow * 0.5f);
    }
}

}  // namespace dxs::theme
