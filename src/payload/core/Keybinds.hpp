#pragma once

#include <Windows.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dxs {

// Declarative keybind registry. Panels and the Overlay query this by action
// name; the user remaps through the Settings panel. Bindings persist via
// Config (key "kb.<action>" = packed string).
class Keybinds {
public:
    struct Binding {
        int  vk    = 0;      // VK_ virtual-key code. 0 = unbound.
        bool ctrl  = false;
        bool shift = false;
        bool alt   = false;

        bool is_bound()            const noexcept { return vk != 0; }
        std::string to_string()    const;
        static Binding from_string(std::string_view s);
    };

    static Keybinds& instance();

    // All known actions — registered at startup with a default binding.
    void register_action(std::string_view name, Binding default_binding,
                         std::string_view display_label);

    Binding get(std::string_view action) const;
    void    set(std::string_view action, Binding b);

    // Called from WndProcHook every WM_KEYDOWN. Returns true when the key
    // matches an action — lets the WndProc decide whether to swallow the
    // event.  The matched action name is written to out_action (empty on no
    // match).
    bool   match(UINT msg, WPARAM wparam, std::string* out_action);

    // Settings UI.
    void   draw_editor();

    // Iteration helper for the Settings panel.
    struct Slot { std::string name; std::string label; Binding binding; };
    const std::vector<Slot>& all() const { return slots_; }

private:
    Keybinds() = default;

    std::vector<Slot>                              slots_;
    std::unordered_map<std::string, size_t>        by_name_;
    bool                                           capturing_ = false;
    std::string                                    capturing_name_;
};

}  // namespace dxs
