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

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "Each button runs the shown snippet inside the game's Python interpreter. "
        "Output streams to the REPL panel. Great for discovering what's importable, "
        "what's loaded, and what global handles the game has lying around.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));

    if (!ready) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::TextUnformatted("● Python bridge not attached — buttons are disabled.");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 6));
    }

    const float avail   = ImGui::GetContentRegionAvail().x;
    const float col_min = 260.0f;
    const int   cols    = std::max(1, int(avail / col_min));
    const float col_w   = (avail - (cols - 1) * 10) / cols;

    ImGui::BeginTable("##qa", cols, ImGuiTableFlags_NoBordersInBody);
    int idx = 0;
    for (const Action& a : k_actions) {
        if ((idx % cols) == 0) ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(idx % cols);

        ImGui::PushID(idx);
        ImGui::BeginChild("##card", ImVec2(col_w, 96), false);

        ImDrawList*  dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetWindowPos();
        const ImVec2 p1 = p0 + ImGui::GetWindowSize();
        dl->AddRectFilled(p0, p1, theme::to_u32(theme::bg_surface), theme::corner_md);

        ImGui::SetCursorPos(ImVec2(14, 10));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
        ImGui::TextUnformatted(a.label);
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(14, 30));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::SetWindowFontScale(0.88f);
        ImGui::TextWrapped("%s", a.subtitle);
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(col_w - 78, 62));
        ImGui::BeginDisabled(!ready);
        if (ImGui::Button("Run", ImVec2(64, 24))) {
            if (auto* repl = python_repl_panel()) {
                repl->submit_external(a.code, /*echo_prompt=*/true);
                ClickGui::instance().select("python_repl");
                ClickGui::instance().toast("snippet → REPL");
            }
        }
        ImGui::EndDisabled();

        ImGui::EndChild();
        ImGui::PopID();
        ++idx;
    }
    ImGui::EndTable();
}

}  // namespace dxs
