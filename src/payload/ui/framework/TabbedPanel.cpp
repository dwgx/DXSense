#include "TabbedPanel.hpp"

#include "Theme.hpp"
#include "core/Config.hpp"

#include <algorithm>
#include <imgui.h>
#include <vector>

namespace dxs {

TabbedPanel::TabbedPanel(std::string id,
                         std::string category,
                         std::string title,
                         std::string icon)
    : id_(std::move(id)),
      category_(std::move(category)),
      title_(std::move(title)),
      icon_(std::move(icon)) {}

void TabbedPanel::add(std::unique_ptr<IPanel> child) {
    if (child) tabs_.push_back(std::move(child));
}

void TabbedPanel::on_first_show() {
    // Restore last-active tab from Config so switching panels doesn't
    // always snap back to the first child.
    const std::string k = "panel." + id_ + ".active_tab";
    active_ = Config::instance().get_int(k, 0);
    active_ = std::clamp(active_, 0, static_cast<int>(tabs_.size()) - 1);
}

void TabbedPanel::draw() {
    if (tabs_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No tabs registered.");
        ImGui::PopStyleColor();
        return;
    }

    // Build the label array for theme::segmented — one label per tab.
    // Labels come from the child's title(). We keep the backing
    // std::string_views alive via the tabs_ vector itself (IPanel::title
    // returns a view into a static or member, valid for the child's
    // lifetime).
    std::vector<const char*> labels;
    labels.reserve(tabs_.size());
    std::vector<std::string> owned;     // stable-address storage for c_str
    owned.reserve(tabs_.size());
    for (const auto& t : tabs_) {
        owned.emplace_back(t->title());
    }
    for (const auto& s : owned) labels.push_back(s.c_str());

    const std::string cfg_key = "panel." + id_ + ".active_tab";
    if (theme::segmented(("##tabs." + id_).c_str(),
                         labels.data(),
                         static_cast<int>(labels.size()),
                         &active_,
                         cfg_key)) {
        // switch fired; also call on_first_show on the newly-selected
        // child so lazy-init (hook installation, etc.) fires.
        if (active_ >= 0 && active_ < static_cast<int>(tabs_.size())) {
            tabs_[active_]->on_first_show();
        }
    }

    // First-time show for the initially-active tab too.
    if (!tabs_first_shown_ && active_ >= 0 &&
        active_ < static_cast<int>(tabs_.size())) {
        tabs_[active_]->on_first_show();
        tabs_first_shown_ = true;
    }

    ImGui::Dummy(ImVec2(0.0f, theme::space_md));

    active_ = std::clamp(active_, 0, static_cast<int>(tabs_.size()) - 1);
    tabs_[active_]->draw();
}

}  // namespace dxs
