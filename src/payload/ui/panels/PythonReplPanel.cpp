#include "PythonReplPanel.hpp"

#include "scripting/PythonBridge.hpp"
#include "ui/framework/Theme.hpp"

#include <Windows.h>
#include <imgui.h>

namespace dxs {

namespace {
constexpr size_t k_max_output_bytes = 256 * 1024;
constexpr size_t k_max_history      = 64;

PythonReplPanel* g_panel_ptr = nullptr;   // for submit_external from other panels
}

void PythonReplPanel::submit(const std::string& src) {
    if (src.empty()) return;
    output_.append(">>> ");
    output_.append(src);
    output_.push_back('\n');
    PythonBridge::instance().exec(src);
    history_.push_back(src);
    while (history_.size() > k_max_history) history_.pop_front();
    history_pos_ = -1;
    scroll_to_bottom_ = true;
}

void PythonReplPanel::submit_external(const std::string& src, bool echo_prompt) {
    if (src.empty()) return;
    if (echo_prompt) {
        output_.append("# ");
        output_.append(src);
        output_.push_back('\n');
    }
    PythonBridge::instance().exec(src);
    scroll_to_bottom_ = true;
}

void PythonReplPanel::draw() {
    g_panel_ptr = this;

    auto& bridge = PythonBridge::instance();

    // Lazy reconnect if the host's Python wasn't up when we first attached.
    if (!bridge.ready()) {
        static int retry_tick = 0;
        if ((++retry_tick % 60) == 0) {
            if (HMODULE h = GetModuleHandleW(L"neox_engine.dll"))
                bridge.initialize(h);
        }
    }

    if (bridge.ready()) {
        std::string chunk = bridge.drain_output();
        if (!chunk.empty()) {
            output_.append(chunk);
            scroll_to_bottom_ = true;
        }
        if (output_.size() > k_max_output_bytes)
            output_.erase(0, output_.size() - k_max_output_bytes);
    }

    ImGui::PushStyleColor(ImGuiCol_Text,
        bridge.ready() ? theme::good : theme::warn);
    ImGui::TextUnformatted(bridge.ready() ? "● interpreter attached"
                                          : "○ interpreter unreachable (retrying)");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("  ↑/↓ history  ·  Ctrl+L clear");
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Scrollable output --------------------------------------------------------
    const float footer_h = ImGui::GetStyle().ItemSpacing.y +
                           ImGui::GetFrameHeightWithSpacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
    ImGui::BeginChild("##py_out", ImVec2(0, -footer_h),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    ImGui::TextUnformatted(output_.data(), output_.data() + output_.size());
    ImGui::PopStyleVar();
    if (scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Input line ---------------------------------------------------------------
    ImGui::PushItemWidth(-FLT_MIN);
    const ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory |
        ImGuiInputTextFlags_EscapeClearsAll;
    auto hist_cb = [](ImGuiInputTextCallbackData* data) -> int {
        auto* self = static_cast<PythonReplPanel*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (self->history_.empty()) return 0;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (self->history_pos_ == -1) self->history_pos_ = int(self->history_.size()) - 1;
                else if (self->history_pos_ > 0) --self->history_pos_;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (self->history_pos_ != -1 &&
                    ++self->history_pos_ >= int(self->history_.size()))
                    self->history_pos_ = -1;
            }
            data->DeleteChars(0, data->BufTextLen);
            if (self->history_pos_ >= 0)
                data->InsertChars(0, self->history_[self->history_pos_].c_str());
        }
        return 0;
    };

    const bool submitted = ImGui::InputText(
        "##py_in", input_, sizeof(input_), flags, hist_cb, this);
    ImGui::PopItemWidth();

    if (submitted) {
        submit(input_);
        input_[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L, false)) output_.clear();
}

PythonReplPanel* python_repl_panel() { return g_panel_ptr; }

}  // namespace dxs
