#include "Notifications.hpp"

#include "Theme.hpp"

#include <algorithm>
#include <imgui.h>

namespace dxs::notify {

namespace {

constexpr double k_fade_in_s   = 0.22;
constexpr double k_fade_out_s  = 0.40;
constexpr float  k_toast_w     = 320.0f;
constexpr float  k_toast_pad_x = 16.0f;
constexpr float  k_toast_pad_y = 12.0f;
constexpr float  k_gap_between = 12.0f;
constexpr float  k_viewport_margin = 24.0f;
constexpr float  k_accent_bar_w    = 3.0f;
constexpr float  k_accent_bar_inset = 8.0f;   // top/bottom inset on the left accent bar
constexpr float  k_icon_slot_w     = 28.0f;

double default_duration(Kind k) {
    // Errors / warnings linger so the user has time to read them.
    return (k == Kind::Error || k == Kind::Warning) ? 5.0 : 3.0;
}

ImVec4 accent_for(Kind k) {
    switch (k) {
        case Kind::Success: return theme::good;
        case Kind::Warning: return theme::warn;
        case Kind::Error:   return theme::bad;
        case Kind::Info:
        default:            return theme::on_surface_variant;
    }
}

void draw_glyph(ImDrawList* dl, ImVec2 c, Kind k, ImU32 col, float alpha) {
    // Four hand-drawn glyphs — we don't require a PUA font here.
    // Scaled to a ~12 px target area.
    col = IM_COL32(
        (col >> IM_COL32_R_SHIFT) & 0xFF,
        (col >> IM_COL32_G_SHIFT) & 0xFF,
        (col >> IM_COL32_B_SHIFT) & 0xFF,
        static_cast<int>(((col >> IM_COL32_A_SHIFT) & 0xFF) * alpha));
    switch (k) {
        case Kind::Info: {
            dl->AddCircleFilled(c, 2.0f, col, 12);
            dl->AddLine(ImVec2(c.x, c.y + 2.0f),
                        ImVec2(c.x, c.y + 7.0f), col, 1.6f);
            break;
        }
        case Kind::Success: {
            dl->AddLine(ImVec2(c.x - 5.0f, c.y + 1.0f),
                        ImVec2(c.x - 1.0f, c.y + 5.0f), col, 1.8f);
            dl->AddLine(ImVec2(c.x - 1.0f, c.y + 5.0f),
                        ImVec2(c.x + 6.0f, c.y - 3.0f), col, 1.8f);
            break;
        }
        case Kind::Warning: {
            // Triangle + exclamation
            dl->AddTriangle(ImVec2(c.x, c.y - 6.0f),
                            ImVec2(c.x - 6.0f, c.y + 5.0f),
                            ImVec2(c.x + 6.0f, c.y + 5.0f),
                            col, 1.4f);
            dl->AddLine(ImVec2(c.x, c.y - 2.0f),
                        ImVec2(c.x, c.y + 2.0f), col, 1.4f);
            dl->AddCircleFilled(ImVec2(c.x, c.y + 4.0f), 0.9f, col, 6);
            break;
        }
        case Kind::Error: {
            dl->AddLine(ImVec2(c.x - 5.0f, c.y - 5.0f),
                        ImVec2(c.x + 5.0f, c.y + 5.0f), col, 1.8f);
            dl->AddLine(ImVec2(c.x + 5.0f, c.y - 5.0f),
                        ImVec2(c.x - 5.0f, c.y + 5.0f), col, 1.8f);
            break;
        }
    }
}

float envelope_alpha(double now, const Notifications::Toast&);

}  // namespace

Notifications& Notifications::instance() {
    static Notifications n;
    return n;
}

void Notifications::push(Kind kind, std::string title, std::string body, double duration) {
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    const double life = (duration > 0.0) ? duration : default_duration(kind);
    toasts_.push_back(Toast{
        kind,
        std::move(title),
        std::move(body),
        now,
        now + life,
        now + life + k_fade_out_s,
    });
    // Cap the visible stack so a spammy producer can't flood the screen.
    constexpr std::size_t kMaxStack = 6;
    if (toasts_.size() > kMaxStack) {
        toasts_.erase(toasts_.begin(),
                      toasts_.begin() + (toasts_.size() - kMaxStack));
    }
}

void Notifications::clear() {
    toasts_.clear();
}

void Notifications::draw() {
    if (toasts_.empty() || !ImGui::GetCurrentContext()) return;

    const double now = ImGui::GetTime();
    std::erase_if(toasts_, [&](const Toast& t) { return t.expire_at <= now; });
    if (toasts_.empty()) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList*    dl = ImGui::GetForegroundDrawList(vp);
    ImFont*        font = ImGui::GetFont();
    const float    title_px = theme::font_body;
    const float    body_px  = theme::font_caption;

    // Stack grows from bottom up, always anchored to the viewport's
    // bottom-right — doesn't care about the ClickGui window's position
    // or size, so toasts stop drifting.
    const float anchor_x = vp->WorkPos.x + vp->WorkSize.x - k_viewport_margin;
    float       y_bottom = vp->WorkPos.y + vp->WorkSize.y - k_viewport_margin;

    // Oldest at the bottom, newest on top — iterate in reverse so the
    // newest toast goes at the current `y_bottom` and older ones stack
    // above it.
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        const Toast& t = *it;

        const float alpha = envelope_alpha(now, t);
        if (alpha <= 0.01f) continue;

        // Measure title + (optional) body to size the card.
        const ImVec2 title_sz = font->CalcTextSizeA(
            title_px, FLT_MAX, 0.0f, t.title.c_str());
        const bool   has_body = !t.body.empty();
        const ImVec2 body_sz  = has_body
            ? font->CalcTextSizeA(body_px, k_toast_w - k_toast_pad_x * 2 -
                                  k_icon_slot_w - k_accent_bar_w, 0.0f,
                                  t.body.c_str())
            : ImVec2(0, 0);

        const float content_h = title_sz.y
            + (has_body ? (body_sz.y + 3.0f) : 0.0f);
        const float toast_h = content_h + k_toast_pad_y * 2;

        const float tl_x = anchor_x - k_toast_w;
        const float tl_y = y_bottom - toast_h;
        const ImVec2 tl{tl_x, tl_y};
        const ImVec2 br{tl_x + k_toast_w, tl_y + toast_h};

        // Subtle lift on fade-in — 6 px slide-up so the toast "arrives"
        // rather than popping in.
        const double life   = now - t.born_at;
        const float  slide  = (life < k_fade_in_s)
            ? (1.0f - static_cast<float>(life / k_fade_in_s)) * 6.0f
            : 0.0f;
        const ImVec2 tl_draw{tl.x, tl.y + slide};
        const ImVec2 br_draw{br.x, br.y + slide};

        // Soft drop shadow — three stacked layers with decaying alpha
        // approximate a Gaussian blur without a shader. Gives the card
        // an unmistakable "floating above the game" lift.
        {
            struct L { float sp; float off; float a; };
            constexpr L layers[] = {
                {1.0f, 1.0f, 0.22f},
                {4.0f, 3.0f, 0.12f},
                {10.0f, 7.0f, 0.05f},
            };
            for (const auto& L : layers) {
                const ImU32 shadow = IM_COL32(0, 0, 0,
                    static_cast<int>(alpha * L.a * 255.0f));
                dl->AddRectFilled(
                    ImVec2(tl_draw.x - L.sp, tl_draw.y - L.sp + L.off),
                    ImVec2(br_draw.x + L.sp, br_draw.y + L.sp + L.off),
                    shadow, theme::radius_md + L.sp);
            }
        }

        // Fill — solid surface_ctn_high so game bg doesn't leak through.
        ImVec4 bg = theme::surface_ctn_high; bg.w *= alpha;
        dl->AddRectFilled(tl_draw, br_draw,
            ImGui::ColorConvertFloat4ToU32(bg), theme::radius_md);

        // 1 px inner highlight on the top edge — the "lit from above"
        // cue that reads as a surface rather than a sticker.
        {
            ImU32 hi = IM_COL32(255, 255, 255, static_cast<int>(alpha * 18.0f));
            dl->AddRect(
                ImVec2(tl_draw.x + 0.5f, tl_draw.y + 0.5f),
                ImVec2(br_draw.x - 0.5f, br_draw.y - 0.5f),
                hi, theme::radius_md, ImDrawFlags_RoundCornersTop, 1.0f);
        }

        // Accent bar (kind-coloured). Not full-height — inset top/bottom
        // to read as an indicator stripe rather than a stacked card edge.
        ImVec4 accent = accent_for(t.kind); accent.w *= alpha;
        dl->AddRectFilled(
            ImVec2(tl_draw.x + 8.0f,                 tl_draw.y + k_accent_bar_inset),
            ImVec2(tl_draw.x + 8.0f + k_accent_bar_w, br_draw.y - k_accent_bar_inset),
            ImGui::ColorConvertFloat4ToU32(accent),
            k_accent_bar_w * 0.5f);

        // Subtle outline.
        ImVec4 border = theme::outline; border.w *= alpha;
        dl->AddRect(tl_draw, br_draw,
            ImGui::ColorConvertFloat4ToU32(border),
            theme::radius_md, 0, 1.0f);

        // Icon glyph in the gutter between accent bar and text.
        const float icon_x_center = tl_draw.x + 8.0f + k_accent_bar_w +
                                    (k_icon_slot_w - 8.0f) * 0.5f;
        const ImVec2 icon_c{icon_x_center,
                            tl_draw.y + k_toast_pad_y + title_sz.y * 0.5f - 1.0f};
        draw_glyph(dl, icon_c, t.kind,
                   ImGui::ColorConvertFloat4ToU32(accent_for(t.kind)), alpha);

        // Title — tighter left margin so title + body align on a single
        // visual column, accent bar + icon sit in their own gutter.
        const float text_x = tl_draw.x + 8.0f + k_accent_bar_w + k_icon_slot_w;
        ImVec4 title_col = theme::on_surface; title_col.w *= alpha;
        dl->AddText(font, title_px,
            ImVec2(text_x, tl_draw.y + k_toast_pad_y),
            ImGui::ColorConvertFloat4ToU32(title_col),
            t.title.c_str());

        // Body — dimmer + on its own line. Wraps to the toast's right edge.
        if (has_body) {
            ImVec4 body_col = theme::on_surface_muted; body_col.w *= alpha;
            dl->AddText(font, body_px,
                ImVec2(text_x,
                       tl_draw.y + k_toast_pad_y + title_sz.y + 4.0f),
                ImGui::ColorConvertFloat4ToU32(body_col),
                t.body.c_str(), nullptr,
                k_toast_w - (text_x - tl_draw.x) - k_toast_pad_x);
        }

        y_bottom = tl.y - k_gap_between;
    }
}

// ─── Envelope ─────────────────────────────────────────────────────────────

namespace {

float envelope_alpha(double now, const Notifications::Toast& t) {
    const double life = now - t.born_at;
    if (life < 0.0) return 0.0f;
    if (life < k_fade_in_s) {
        const float s = static_cast<float>(life / k_fade_in_s);
        // Ease-out cubic
        const float inv = 1.0f - s;
        return 1.0f - inv * inv * inv;
    }
    if (now < t.fade_at) return 1.0f;
    const double out = now - t.fade_at;
    if (out >= k_fade_out_s) return 0.0f;
    const float s = static_cast<float>(out / k_fade_out_s);
    return 1.0f - s;
}

}  // namespace

// ─── Convenience forwarders ───────────────────────────────────────────────

void info   (std::string t, std::string b) { Notifications::instance().push(Kind::Info,    std::move(t), std::move(b)); }
void success(std::string t, std::string b) { Notifications::instance().push(Kind::Success, std::move(t), std::move(b)); }
void warn   (std::string t, std::string b) { Notifications::instance().push(Kind::Warning, std::move(t), std::move(b)); }
void error  (std::string t, std::string b) { Notifications::instance().push(Kind::Error,   std::move(t), std::move(b)); }

}  // namespace dxs::notify
