#pragma once

#include <string_view>

namespace dxs {

// All ClickGui panels implement this. Registration happens once at startup;
// the ClickGui keeps the concrete pointer and calls draw() when the panel is
// the selected one in the sidebar.
//
// Design note: panels own their own state (counters, scroll, selections).
// They never store ImGui widget state across frames — always read back from
// ImGui each frame. That way a panel is safe to hide/show without leaking.
class IPanel {
public:
    virtual ~IPanel() = default;

    // Short stable identifier — used for config persistence, not for display.
    virtual std::string_view id() const = 0;

    // Category groups the panel in the sidebar. Empty string = root level.
    virtual std::string_view category() const = 0;

    // One-line label shown in the sidebar and as the content-area header.
    virtual std::string_view title() const = 0;

    // Called once when the panel is first selected (for deferred setup like
    // buffer allocation). Default no-op.
    virtual void on_first_show() {}

    // Called every frame the panel is visible. Panel draws into the current
    // ImGui window — do NOT begin/end a window here, ClickGui owns the frame.
    virtual void draw() = 0;
};

}  // namespace dxs
