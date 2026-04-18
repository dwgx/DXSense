#include "Tick.hpp"

#include "Procedure.hpp"

#include <imgui.h>

namespace dxs::procedure {

double Tick::now() const {
    return ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
}

void Tick::python(std::string_view snippet) {
    if (snippet.empty()) return;
    PythonIntent i;
    i.source.assign(snippet);
    if (origin_) i.origin.assign(origin_->manifest().handle);
    python_.push_back(std::move(i));
}

void Tick::emit(std::string_view channel, std::string_view payload) {
    if (channel.empty()) return;
    EventIntent i;
    i.channel.assign(channel);
    i.payload.assign(payload);
    if (origin_) i.origin.assign(origin_->manifest().handle);
    events_.push_back(std::move(i));
}

}  // namespace dxs::procedure
