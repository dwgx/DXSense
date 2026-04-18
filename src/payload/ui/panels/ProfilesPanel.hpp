#pragma once

#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/MdiIcons.hpp"

namespace dxs {

// Dedicated page for named Config snapshots — list, switch, save, save-as,
// delete, import, export. The Modules panel used to carry a compressed
// version of this as a top-of-page bar; that bar was removed so Modules
// stays focused on per-procedure cards. Also labels each entry with its
// provenance (local / cloud) so the user knows where a profile lives.
class ProfilesPanel : public IPanel {
public:
    std::string_view id()       const override { return "profiles"; }
    std::string_view category() const override { return "Scripting"; }
    std::string_view title()    const override { return "Profiles"; }
    std::string_view icon()     const override { return MDI_FILE_DOCUMENT_OUTLINE; }
    void             draw()           override;
};

}  // namespace dxs
