#include "Pin.hpp"

#include "ui/framework/Theme.hpp"

#include <Windows.h>
#include <cstdio>
#include <imgui.h>
#include <string>
#include <vector>

// Render-side of Pin — concentrated here so Pin.hpp stays theme-free and
// can be included from Procedure implementations that don't care about UI.

namespace dxs::procedure {

void PinBool::draw() {
    bool v = value_;
    if (theme::checkbox(std::string(label()).c_str(), &v)) {
        set(v);
    }
}

void PinFloat::draw() {
    float v = value_;
    const std::string key = std::string("##") + config_key_;
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(std::string(label()).c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();
    if (theme::slider_float(key.c_str(), &v, min(), max(), "%.2f", 0.0f)) {
        set(v);
    }
}

void PinInt::draw() {
    int v = value_;
    const std::string key = std::string("##") + config_key_;
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(std::string(label()).c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();
    if (theme::slider_int(key.c_str(), &v, min(), max(), 0.0f)) {
        set(v);
    }
}

void PinChoice::draw() {
    int v = value_;
    const std::string key = std::string("##") + config_key_;
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(std::string(label()).c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();
    if (theme::segmented(key.c_str(), options_.data(), count_, &v)) {
        set(v);
    }
}

// ─── PinString ──────────────────────────────────────────────────────────

void PinString::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(std::string(label()).c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    // ImGui::InputText needs a char buffer; we stage to a growable vector
    // sized to the Pin's declared max_len_ + null terminator.
    std::vector<char> buf(max_len_ + 1, 0);
    const std::size_t n = std::min(value_.size(), max_len_);
    std::memcpy(buf.data(), value_.data(), n);
    buf[n] = '\0';

    const std::string id = std::string("##str_") + config_key_;
    if (ImGui::GetContentRegionAvail().x > 220.0f)
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme::radius_md);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4{0.10f, 0.10f, 0.105f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.14f, 0.14f, 0.145f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4{0.16f, 0.16f, 0.165f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Border,         theme::outline);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (ImGui::InputText(id.c_str(), buf.data(), buf.size())) {
        set(std::string_view(buf.data()));
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

// ─── PinKey ─────────────────────────────────────────────────────────────

namespace {
// Listening state — one key at a time across the whole app. Stored as the
// PinKey instance pointer so multiple PinKeys on a page don't interfere.
const void* g_key_listening_for = nullptr;

// Keys we treat as "cancel listening" rather than a valid bind.
bool is_listen_cancel(int vk) {
    return vk == VK_ESCAPE || vk == VK_LWIN || vk == VK_RWIN;
}
}  // namespace

const char* PinKey::label_for(int vk) {
    if (vk == 0) return "(unbound)";
    // Special names first — GetKeyNameText is fine for letters/digits but
    // names things like "F7" and "Space" with its own spelling quirks.
    switch (vk) {
        case VK_ESCAPE:   return "Esc";
        case VK_SPACE:    return "Space";
        case VK_RETURN:   return "Enter";
        case VK_TAB:      return "Tab";
        case VK_BACK:     return "Backspace";
        case VK_LSHIFT:   return "LShift";
        case VK_RSHIFT:   return "RShift";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_LMENU:    return "LAlt";
        case VK_RMENU:    return "RAlt";
        case VK_UP:       return "Up";
        case VK_DOWN:     return "Down";
        case VK_LEFT:     return "Left";
        case VK_RIGHT:    return "Right";
        case VK_INSERT:   return "Insert";
        case VK_DELETE:   return "Delete";
        case VK_HOME:     return "Home";
        case VK_END:      return "End";
        case VK_PRIOR:    return "PgUp";
        case VK_NEXT:     return "PgDn";
        default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24) {
        static char buf[8];
        std::snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
        return buf;
    }
    if (vk >= '0' && vk <= '9') {
        static char buf[2] = {0,0}; buf[0] = static_cast<char>(vk); return buf;
    }
    if (vk >= 'A' && vk <= 'Z') {
        static char buf[2] = {0,0}; buf[0] = static_cast<char>(vk); return buf;
    }
    static char num[8];
    std::snprintf(num, sizeof(num), "VK 0x%02X", vk);
    return num;
}

void PinKey::draw() {
    // PushID on config_key_ — guarantees each PinKey has its own ImGui
    // ID scope regardless of button body text. Prior to this fix every
    // unbound PinKey rendered its button as "Sigil: (unbound)", and
    // theme::ghost_button's internal PushID(body) collapsed them to the
    // same ID → clicks never routed to the right PinKey instance so
    // "无法绑定 Hotkey".
    ImGui::PushID(config_key_.c_str());

    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(std::string(label()).c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    const bool listening = (g_key_listening_for == this);
    char body[48];
    if (listening) std::snprintf(body, sizeof(body), "Press a key…  (Esc = cancel)");
    else           std::snprintf(body, sizeof(body), "Sigil: %s", label_for(value_));

    if (theme::ghost_button(body, ImVec2(200.0f, 28.0f))) {
        g_key_listening_for = listening ? nullptr : this;
    }

    if (listening) {
        // Scan the keyboard for the first down key this frame. Native VK
        // range is [0x01..0xFE]; we skip mouse buttons AND modifier-only
        // keys so holding Shift/Ctrl while clicking the button doesn't
        // bind the modifier itself.
        auto is_modifier_only = [](int vk) {
            return vk == VK_SHIFT   || vk == VK_CONTROL || vk == VK_MENU ||
                   vk == VK_LSHIFT  || vk == VK_RSHIFT  ||
                   vk == VK_LCONTROL|| vk == VK_RCONTROL||
                   vk == VK_LMENU   || vk == VK_RMENU;
        };
        for (int vk = 0x07; vk < 0xFF; ++vk) {
            if (is_modifier_only(vk)) continue;
            if ((GetAsyncKeyState(vk) & 0x8000) == 0) continue;
            g_key_listening_for = nullptr;
            if (is_listen_cancel(vk)) {
                // Esc cancels the bind, doesn't set one.
            } else {
                set(vk);
            }
            break;
        }
    }

    ImGui::PopID();
}

// ─── PinColor ───────────────────────────────────────────────────────────

namespace {
int hex_byte(std::string_view s, std::size_t idx) {
    auto d = [](char c)->int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
    };
    if (idx + 1 >= s.size()) return -1;
    const int hi = d(s[idx]);
    const int lo = d(s[idx + 1]);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}
}  // namespace

std::string PinColor::pack(ImVec4 c) {
    auto b = [](float v)->int { return std::clamp(int(v * 255.0f + 0.5f), 0, 255); };
    char out[12];
    std::snprintf(out, sizeof(out), "#%02x%02x%02x%02x",
                  b(c.x), b(c.y), b(c.z), b(c.w));
    return out;
}

ImVec4 PinColor::unpack(std::string_view s, ImVec4 fallback) {
    // Accept "#rrggbbaa" or "#rrggbb" (alpha defaults to 255).
    if (s.empty() || s[0] != '#') return fallback;
    const std::size_t len = s.size() - 1;
    if (len != 6 && len != 8) return fallback;
    const int r = hex_byte(s, 1);
    const int g = hex_byte(s, 3);
    const int b = hex_byte(s, 5);
    int a = 255;
    if (len == 8) a = hex_byte(s, 7);
    if (r < 0 || g < 0 || b < 0 || a < 0) return fallback;
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void PinColor::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(std::string(label()).c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    float c[4] = {value_.x, value_.y, value_.z, value_.w};
    const std::string id = std::string("##col_") + config_key_;
    if (ImGui::GetContentRegionAvail().x > 220.0f)
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    const ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_DisplayHex |
        ImGuiColorEditFlags_PickerHueBar |
        ImGuiColorEditFlags_NoInputs;
    if (ImGui::ColorEdit4(id.c_str(), c, flags)) {
        set(ImVec4(c[0], c[1], c[2], c[3]));
    }
}

}  // namespace dxs::procedure
