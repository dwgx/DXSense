#pragma once

#include "ui/framework/IPanel.hpp"

#include <cstdint>
#include <string>

namespace dxs {

// Minimal hex viewer. Not a write-patch tool — strictly read-only so you can't
// accidentally corrupt live game memory from here.
class MemoryPanel : public IPanel {
public:
    std::string_view id()       const override { return "memory"; }
    std::string_view category() const override { return "Analysis"; }
    std::string_view title()    const override { return "Memory"; }
    void             draw()           override;

private:
    void ensure_base_set();
    void try_parse_address();

    char          input_[64]{};
    uint64_t      base_addr_  = 0;
    int           size_bytes_ = 512;
    bool          follow_rip_ = false;
    std::string   last_error_;
};

}  // namespace dxs
