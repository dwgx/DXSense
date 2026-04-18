#include "MemoryPanel.hpp"

#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "ui/framework/Theme.hpp"

#include <Windows.h>
#include <imgui.h>

#include <cctype>
#include <cstdio>

namespace dxs {

void MemoryPanel::ensure_base_set() {
    if (base_addr_ == 0) {
        // Restore from config first — the user's last-inspected address is
        // usually where they want to come back to.
        const auto saved = Config::instance().get("memory.address");
        if (!saved.empty()) {
            std::strncpy(input_, saved.c_str(), sizeof(input_) - 1);
            try_parse_address();
        }
        if (base_addr_ == 0) {
            // First-time default: neox_engine.dll image base.
            if (HMODULE h = GetModuleHandleW(L"neox_engine.dll"))
                base_addr_ = reinterpret_cast<uint64_t>(h);
            else
                base_addr_ = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
            std::snprintf(input_, sizeof(input_), "0x%llX",
                          static_cast<unsigned long long>(base_addr_));
        }
        size_bytes_ = Config::instance().get_int("memory.size", 512);
    }
}

void MemoryPanel::try_parse_address() {
    last_error_.clear();
    const char* s = input_;
    while (*s && std::isspace(static_cast<unsigned char>(*s))) ++s;
    char*      end = nullptr;
    const auto v   = std::strtoull(s, &end, 0);
    if (end == s) { last_error_ = "not a valid integer"; return; }
    base_addr_ = v;
    Config::instance().set("memory.address", input_);
}

void MemoryPanel::draw() {
    ensure_base_set();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("memory.intro").data());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // Control row -------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted(L("common.address").data());
    ImGui::PopStyleColor();

    ImGui::PushItemWidth(240);
    if (ImGui::InputText("##addr", input_, sizeof(input_),
                         ImGuiInputTextFlags_EnterReturnsTrue |
                         ImGuiInputTextFlags_AutoSelectAll)) {
        try_parse_address();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Go")) try_parse_address();

    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    if (ImGui::SliderInt("##len", &size_bytes_, 64, 4096, "len=%d")) {
        Config::instance().set_int("memory.size", size_bytes_);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::SmallButton("-0x100")) base_addr_ -= 0x100;
    ImGui::SameLine();
    if (ImGui::SmallButton("+0x100")) base_addr_ += 0x100;
    ImGui::SameLine();
    if (ImGui::SmallButton("neox_engine base")) {
        base_addr_ = reinterpret_cast<uint64_t>(GetModuleHandleW(L"neox_engine.dll"));
        std::snprintf(input_, sizeof(input_), "0x%llX",
                      static_cast<unsigned long long>(base_addr_));
    }

    if (!last_error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
        ImGui::Text("! %s", last_error_.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // Hex dump with safe read. IsBadReadPtr is the classic "just check" — we
    // don't need a precise VirtualQuery because we render page-by-page anyway.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
    ImGui::BeginChild("##mem_view", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    constexpr int cols = 16;
    char line[200];
    const auto* p = reinterpret_cast<const uint8_t*>(base_addr_);
    for (int row = 0; row * cols < size_bytes_; ++row) {
        MEMORY_BASIC_INFORMATION mbi{};
        const auto probe = p + row * cols;
        bool readable = VirtualQuery(probe, &mbi, sizeof(mbi)) != 0
                        && mbi.State == MEM_COMMIT
                        && !(mbi.Protect & PAGE_NOACCESS)
                        && !(mbi.Protect & PAGE_GUARD);

        std::snprintf(line, sizeof(line), "%016llX  ",
                      static_cast<unsigned long long>(base_addr_ + row * cols));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::accent);
        ImGui::TextUnformatted(line);
        ImGui::PopStyleColor();
        ImGui::SameLine();

        std::string hex, ascii;
        for (int c = 0; c < cols; ++c) {
            if (row * cols + c >= size_bytes_) break;
            char buf[8];
            if (readable) {
                uint8_t b = probe[c];
                std::snprintf(buf, sizeof(buf), "%02X ", b);
                hex += buf;
                ascii += (b >= 32 && b < 127) ? static_cast<char>(b) : '.';
            } else {
                hex += "?? ";
                ascii += '?';
            }
        }
        ImGui::PushStyleColor(ImGuiCol_Text,
                              readable ? theme::text_primary : theme::text_faded);
        ImGui::TextUnformatted(hex.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted(ascii.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace dxs
