#include "Pin.hpp"

#include "ui/framework/Theme.hpp"

#include <imgui.h>

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

}  // namespace dxs::procedure
