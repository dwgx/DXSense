#include "Keybinds.hpp"

#include "Config.hpp"
#include "Localization.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace dxs {

namespace {
std::string vk_name(int vk) {
    if (vk == 0) return "none";
    // A fair subset of named keys. Fall back to VK_<decimal> so the string
    // round-trips cleanly even for F13 or obscure OEM keys.
    switch (vk) {
        case VK_INSERT: return "Insert"; case VK_DELETE: return "Delete";
        case VK_HOME:   return "Home";   case VK_END:    return "End";
        case VK_PRIOR:  return "PageUp"; case VK_NEXT:   return "PageDown";
        case VK_UP:     return "Up";     case VK_DOWN:   return "Down";
        case VK_LEFT:   return "Left";   case VK_RIGHT:  return "Right";
        case VK_TAB:    return "Tab";    case VK_ESCAPE: return "Esc";
        case VK_RETURN: return "Enter";  case VK_SPACE:  return "Space";
        case VK_BACK:   return "Backspace";
        case VK_F1:  return "F1";  case VK_F2:  return "F2";
        case VK_F3:  return "F3";  case VK_F4:  return "F4";
        case VK_F5:  return "F5";  case VK_F6:  return "F6";
        case VK_F7:  return "F7";  case VK_F8:  return "F8";
        case VK_F9:  return "F9";  case VK_F10: return "F10";
        case VK_F11: return "F11"; case VK_F12: return "F12";
        case VK_OEM_3: return "`";
    }
    if (vk >= '0' && vk <= '9') { char b[2]={char(vk),0}; return b; }
    if (vk >= 'A' && vk <= 'Z') { char b[2]={char(vk),0}; return b; }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "VK_%d", vk);
    return buf;
}

int vk_from_name(std::string_view s) {
    if (s == "none")    return 0;
    if (s == "Insert")  return VK_INSERT; if (s == "Delete")    return VK_DELETE;
    if (s == "Home")    return VK_HOME;   if (s == "End")       return VK_END;
    if (s == "PageUp")  return VK_PRIOR;  if (s == "PageDown")  return VK_NEXT;
    if (s == "Up")      return VK_UP;     if (s == "Down")      return VK_DOWN;
    if (s == "Left")    return VK_LEFT;   if (s == "Right")     return VK_RIGHT;
    if (s == "Tab")     return VK_TAB;    if (s == "Esc")       return VK_ESCAPE;
    if (s == "Enter")   return VK_RETURN; if (s == "Space")     return VK_SPACE;
    if (s == "Backspace") return VK_BACK;
    for (int i = 1; i <= 12; ++i) {
        char b[4]; std::snprintf(b, sizeof(b), "F%d", i);
        if (s == b) return VK_F1 + (i - 1);
    }
    if (s.size() == 1 && s[0] >= 'A' && s[0] <= 'Z') return s[0];
    if (s.size() == 1 && s[0] >= '0' && s[0] <= '9') return s[0];
    if (s.rfind("VK_", 0) == 0) return std::atoi(s.data() + 3);
    return 0;
}
}  // namespace

std::string Keybinds::Binding::to_string() const {
    std::string out;
    if (ctrl)  out += "Ctrl+";
    if (shift) out += "Shift+";
    if (alt)   out += "Alt+";
    out += vk_name(vk);
    return out;
}

Keybinds::Binding Keybinds::Binding::from_string(std::string_view s) {
    Binding b;
    std::string buf(s);
    while (true) {
        auto plus = buf.find('+');
        if (plus == std::string::npos) break;
        const std::string tok = buf.substr(0, plus);
        if      (tok == "Ctrl")  b.ctrl = true;
        else if (tok == "Shift") b.shift = true;
        else if (tok == "Alt")   b.alt = true;
        buf = buf.substr(plus + 1);
    }
    b.vk = vk_from_name(buf);
    return b;
}

Keybinds& Keybinds::instance() {
    static Keybinds k;
    return k;
}

void Keybinds::register_action(std::string_view name, Binding def,
                               std::string_view label) {
    const auto n = std::string(name);
    if (by_name_.count(n)) return;

    // Persistence: load from config if present, otherwise default.
    const auto stored = Config::instance().get("kb." + n);
    const Binding b = stored.empty() ? def : Binding::from_string(stored);
    if (stored.empty())
        Config::instance().set("kb." + n, b.to_string());

    by_name_[n] = slots_.size();
    slots_.push_back({n, std::string(label), b});
}

Keybinds::Binding Keybinds::get(std::string_view name) const {
    auto it = by_name_.find(std::string(name));
    return it != by_name_.end() ? slots_[it->second].binding : Binding{};
}

void Keybinds::set(std::string_view name, Binding b) {
    auto it = by_name_.find(std::string(name));
    if (it == by_name_.end()) return;
    slots_[it->second].binding = b;
    Config::instance().set("kb." + std::string(name), b.to_string());
}

bool Keybinds::match(UINT msg, WPARAM wparam, std::string* out_action) {
    if (out_action) out_action->clear();
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;

    Binding b;
    b.vk    = static_cast<int>(wparam);
    b.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    b.shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    b.alt   = (GetKeyState(VK_MENU)    & 0x8000) != 0;

    for (const Slot& s : slots_) {
        const Binding& t = s.binding;
        if (t.vk == b.vk && t.ctrl == b.ctrl &&
            t.shift == b.shift && t.alt == b.alt) {
            if (out_action) *out_action = s.name;
            return true;
        }
    }
    return false;
}

void Keybinds::draw_editor() {
    // Settings panel embeds this. Each row: label | current binding button.
    if (ImGui::BeginTable("##kb", 2, ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_BordersInnerV |
                                     ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableHeadersRow();

        for (Slot& s : slots_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(s.label.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::PushID(s.name.c_str());
            const bool listening = capturing_ && capturing_name_ == s.name;
            const std::string label = listening ? "press a key..."
                                                : s.binding.to_string();
            if (ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0))) {
                capturing_      = !listening;
                capturing_name_ = listening ? "" : s.name;
            }
            if (listening) {
                ImGuiIO& io = ImGui::GetIO();
                for (int vk = 1; vk < 255; ++vk) {
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(vk), false))
                        {}  // ImGui key mapping diverges from VK; we rely on
                            // raw WM_KEYDOWN capture via a peek below.
                }
                // Simpler capture: check GetAsyncKeyState for each VK.
                for (int vk = 1; vk < 255; ++vk) {
                    if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU) continue;
                    if (vk == VK_LBUTTON || vk == VK_RBUTTON ||
                        vk == VK_MBUTTON) continue;
                    if (GetAsyncKeyState(vk) & 0x0001) {
                        Binding b;
                        b.vk    = vk;
                        b.ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        b.shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
                        b.alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
                        set(s.name, b);
                        capturing_      = false;
                        capturing_name_ = "";
                        break;
                    }
                }
                (void)io;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

}  // namespace dxs
