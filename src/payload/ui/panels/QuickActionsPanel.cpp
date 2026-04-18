#include "QuickActionsPanel.hpp"

#include "PythonReplPanel.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

// Forward — lives in PythonReplPanel.cpp.
PythonReplPanel* python_repl_panel();

namespace {

struct Action {
    const char* label;
    const char* subtitle;
    const char* code;
};

// Curated. Every snippet is idempotent, safe, and fits on one line so we can
// show the user exactly what's about to run.
constexpr Action k_actions[] = {
    {"sys.path",
     "Where the game looks for Python modules",
     "import sys; [print(' ', p) for p in sys.path]"},
    {"Loaded engine modules",
     "Anything under neox/mobile/common namespaces",
     "import sys; print('\\n'.join(sorted(m for m in sys.modules if any(m.startswith(x) for x in ('neox','mobile','common','engine')))))"},
    {"__main__ globals",
     "Top-level names in the game's main interpreter",
     "import __main__; print('\\n'.join(n for n in dir(__main__) if not n.startswith('__')))"},
    {"Interpreter info",
     "CPython version, implementation flags",
     "import sys; print(sys.version); print('prefix =', sys.prefix); print('base_exec_prefix =', sys.base_exec_prefix)"},
    {"Installed sys hooks",
     "meta_path + path_hooks (who can import what)",
     "import sys; print('meta_path:'); [print(' ', h) for h in sys.meta_path]; print('path_hooks:'); [print(' ', h) for h in sys.path_hooks]"},
    {"neox built-ins",
     "Try `import neox` and list attributes if present",
     "try:\n    import neox; print(sorted(a for a in dir(neox) if not a.startswith('_')))\nexcept Exception as e: print('neox not importable:', e)"},
    {"Threads snapshot",
     "Which threads are alive inside the interpreter",
     "import threading; [print(f'  {t.name:30}  daemon={t.daemon}') for t in threading.enumerate()]"},
    {"GC objects sample",
     "Top 10 classes by live instance count",
     "import gc, collections; c = collections.Counter(type(o).__name__ for o in gc.get_objects()); [print(f'  {n:<40} {k}') for n,k in c.most_common(10)]"},
};

}  // namespace

void QuickActionsPanel::draw() {
    const bool ready = PythonBridge::instance().ready();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    theme::section_label("SCRIPT SNIPPETS");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "Each button runs the shown snippet inside the game's Python interpreter. "
        "Output streams to the REPL panel. Great for discovering what's importable, "
        "what's loaded, and what global handles the game has lying around.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    if (!ready) {
        theme::badge(theme::Status::Warn, "Python bridge offline");
        ImGui::SameLine(0, theme::space_sm);
        theme::status_chip(theme::Status::Warn,
                           "buttons disabled until CPython exports are attached");
        ImGui::Dummy(ImVec2(0, theme::space_sm));
    }

    const float run_w  = 64.0f;

    theme::card_grid(std::begin(k_actions), std::end(k_actions), theme::card_h_md,
        [&](const Action& a, const theme::CardGridCell& c) {
        dl->AddRectFilled(c.tl, c.br,
                          theme::to_u32(theme::surface_ctn), theme::radius_lg);
        dl->AddRect(c.tl, c.br,
                    theme::to_u32(theme::outline), theme::radius_lg, 0, 1.0f);

        ImGui::SetCursorScreenPos(c.tl + ImVec2(theme::card_pad_x, theme::card_pad_y));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::SetWindowFontScale(theme::scale_header);
        ImGui::TextUnformatted(a.label);
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(c.tl + ImVec2(
            theme::card_pad_x,
            theme::card_pad_y + ImGui::GetTextLineHeight() + theme::space_xs));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::PushTextWrapPos(c.br.x - theme::card_pad_x - run_w - theme::space_sm);
        ImGui::TextUnformatted(a.subtitle);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(c.tl + ImVec2(
            c.w - theme::card_pad_x - run_w,
            theme::card_h_md - theme::card_pad_y - theme::control_h_sm));
        ImGui::BeginDisabled(!ready);
        ImGui::PushStyleColor(ImGuiCol_Button,        theme::primary_container);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::surface_ctn_highest);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  theme::surface_ctn_highest);
        ImGui::PushStyleColor(ImGuiCol_Text,          theme::on_surface);
        if (ImGui::Button("Run", ImVec2(run_w, theme::control_h_sm))) {
            if (auto* repl = python_repl_panel()) {
                repl->submit_external(a.code, /*echo_prompt=*/true);
                ClickGui::instance().select("python_repl");
                ClickGui::instance().toast("snippet → REPL");
            }
        }
        ImGui::PopStyleColor(4);
        ImGui::EndDisabled();
    });
}

}  // namespace dxs
