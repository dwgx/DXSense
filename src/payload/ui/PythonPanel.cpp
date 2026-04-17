#include "PythonPanel.hpp"

#include "scripting/PythonBridge.hpp"

#include <Windows.h>
#include <imgui.h>

#include <cstring>

namespace dxs {

namespace {
constexpr size_t k_max_output_bytes = 256 * 1024;
constexpr size_t k_max_history      = 64;
}

PythonPanel& PythonPanel::instance() {
    static PythonPanel p;
    return p;
}

PythonPanel::PythonPanel() {
    output_.reserve(8 * 1024);
}

void PythonPanel::submit(const std::string& src) {
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

void PythonPanel::draw() {
    if (!visible_) return;

    // Lazy re-initialise: if the host's CPython wasn't initialised when the
    // engine first tried to attach (common when injecting at game launch
    // before the game loads its Python layer), retry every 60 frames.
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) {
        static int retry_tick = 0;
        if ((++retry_tick % 60) == 0) {
            if (HMODULE py_host = GetModuleHandleW(L"neox_engine.dll")) {
                bridge.initialize(py_host);
            }
        }
    }

    // Drain Python -> append to local buffer. We do it every frame so that
    // async prints from the game's own Python threads (which also redirect
    // through our sink once the bridge is online) also surface here.
    if (bridge.ready()) {
        std::string chunk = bridge.drain_output();
        if (!chunk.empty()) {
            output_.append(chunk);
            scroll_to_bottom_ = true;
        }
        // Trim from the front if we've blown past the cap.
        if (output_.size() > k_max_output_bytes) {
            output_.erase(0, output_.size() - k_max_output_bytes);
        }
    }

    ImGui::SetNextWindowSize(ImVec2(720, 460), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Python / NeoX3 REPL", &visible_)) {
        ImGui::End();
        return;
    }

    const bool ready = bridge.ready();
    ImGui::TextColored(ready ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f)
                             : ImVec4(0.9f, 0.5f, 0.3f, 1.0f),
                       ready ? "interpreter: attached" : "interpreter: not attached");
    ImGui::SameLine();
    ImGui::TextDisabled("| ↑/↓ history | Ctrl+L clear");

    ImGui::Separator();

    // Output area ----------------------------------------------------------
    const float footer_h = ImGui::GetStyle().ItemSpacing.y +
                           ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("##out", ImVec2(0, -footer_h),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        // TextUnformatted skips per-char word-wrap + format parsing → 10x
        // faster than TextWrapped for large console buffers.
        ImGui::TextUnformatted(output_.data(), output_.data() + output_.size());
        ImGui::PopStyleVar();

        if (scroll_to_bottom_) {
            ImGui::SetScrollHereY(1.0f);
            scroll_to_bottom_ = false;
        }
    }
    ImGui::EndChild();

    // Input area -----------------------------------------------------------
    ImGui::PushItemWidth(-FLT_MIN);
    const ImGuiInputTextFlags flags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_CallbackHistory |
        ImGuiInputTextFlags_EscapeClearsAll;

    auto hist_cb = [](ImGuiInputTextCallbackData* data) -> int {
        auto* self = static_cast<PythonPanel*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (self->history_.empty()) return 0;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (self->history_pos_ == -1) self->history_pos_ = int(self->history_.size()) - 1;
                else if (self->history_pos_ > 0) --self->history_pos_;
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (self->history_pos_ != -1 && ++self->history_pos_ >= int(self->history_.size()))
                    self->history_pos_ = -1;
            }
            data->DeleteChars(0, data->BufTextLen);
            if (self->history_pos_ >= 0)
                data->InsertChars(0, self->history_[self->history_pos_].c_str());
        }
        return 0;
    };

    const bool submitted = ImGui::InputText(
        "##in", input_, sizeof(input_), flags, hist_cb, this);
    ImGui::PopItemWidth();

    if (submitted) {
        submit(input_);
        input_[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }

    // Hotkey: Ctrl+L clears the console
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L, false)) {
        output_.clear();
    }

    ImGui::End();
}

}  // namespace dxs
