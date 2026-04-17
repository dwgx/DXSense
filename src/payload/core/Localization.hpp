#pragma once

#include <string>
#include <string_view>

namespace dxs {

// Minimal i18n. Two languages shipped: "en" (key-identity) and "zh-CN".
// Unknown keys fall back to the key itself — we never return empty strings,
// so panels always display *something* even during translation lag.
class Localization {
public:
    static Localization& instance();

    // "en" or "zh-CN". Safe to call at any time; the returned strings are
    // valid until the next language change.
    void              set_language(std::string_view code);
    std::string_view  language() const;

    std::string_view  t(std::string_view key) const;

private:
    Localization();
    std::string lang_;
};

}  // namespace dxs

// Short macro — every user-facing string should route through this.
#define L(key) ::dxs::Localization::instance().t(key)
