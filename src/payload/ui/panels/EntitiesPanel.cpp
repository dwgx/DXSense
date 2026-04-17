#include "EntitiesPanel.hpp"

#include "PythonReplPanel.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

PythonReplPanel* python_repl_panel();   // from PythonReplPanel.cpp

void EntitiesPanel::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "Goal: live list of every game entity (Survivors / Hunter / props / "
        "repair machines / gates) with HP, position, and the RPC mailbox handle "
        "for each. The enumeration hook into EntityManager is pending the "
        "subsystem survey — in the meantime, you can probe the Python side "
        "directly with the snippets below.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    struct Probe { const char* label; const char* code; };
    constexpr Probe probes[] = {
        {"Python-visible entity modules",
         "import sys; print('\\n'.join(sorted(m for m in sys.modules if 'entity' in m.lower() or 'avatar' in m.lower() or 'soul' in m.lower())))"},
        {"Globals with 'mgr' / 'manager'",
         "import __main__; print('\\n'.join(n for n in dir(__main__) if 'mgr' in n.lower() or 'manager' in n.lower()))"},
        {"Try neox.entity",
         "try:\n    import neox.entity as e; print(sorted(a for a in dir(e) if not a.startswith('_')))\nexcept Exception as ex: print('neox.entity:', ex)"},
        {"weakref live-entity scan",
         "import gc, weakref; cnt = sum(1 for o in gc.get_objects() if type(o).__name__.endswith('Entity')); print('live *Entity instances:', cnt)"},
    };

    for (const Probe& p : probes) {
        ImGui::BulletText("%s", p.label);
        ImGui::SameLine(0, 12);
        ImGui::PushID(p.label);
        if (ImGui::SmallButton("Run")) {
            if (auto* repl = python_repl_panel()) {
                repl->submit_external(p.code, true);
                ClickGui::instance().select("python_repl");
                ClickGui::instance().toast("probe → REPL");
            }
        }
        ImGui::PopID();
    }
}

}  // namespace dxs
