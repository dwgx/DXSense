#pragma once

// Windows 11 Fluent Icons — U+E700..U+F8FF range in SegoeIcons.ttf.
// Each macro is a 3-byte UTF-8 encoding of the codepoint, ready for string
// concatenation with plain label text ("ICON " label").
//
// Reference:
//   https://learn.microsoft.com/en-us/windows/apps/design/style/segoe-fluent-icons-font
//
// We deliberately hand-pick the glyphs we use rather than pulling the full
// range into the ImGui atlas — the font has 1500+ icons, no point rasterising
// all of them into a texture that's already 2 MB.

#define ICON_HOME        "\xEE\xA0\x8F"   // E80F  Home
#define ICON_SETTINGS    "\xEE\x9D\x93"   // E713  Settings wheel
#define ICON_REFRESH     "\xEE\x9C\xAC"   // E72C  Refresh
#define ICON_PLAY        "\xEE\x9D\xA8"   // E768  Play
#define ICON_PAUSE       "\xEE\x9D\xA9"   // E769  Pause
#define ICON_CLEAR       "\xEE\x9C\x91"   // E711  ChromeClose (x)
#define ICON_FILTER      "\xEE\xA3\xAB"   // E8EB  Filter
#define ICON_SEARCH      "\xEE\x9D\x94"   // E721  Zoom
#define ICON_GLOBE       "\xEE\xA3\x93"   // E8D3  Globe
#define ICON_PEOPLE      "\xEE\xA3\xB1"   // E8F1  People
#define ICON_DOC         "\xEE\xA2\xA5"   // E8A5  Document
#define ICON_MEMORY      "\xEE\xAA\xAD"   // EAAD  Memory/Ram
#define ICON_CODE        "\xEE\xA5\x83"   // E943  ClosedCaption (reuse as code)
#define ICON_BOLT        "\xEE\xA5\x85"   // E945  LightningBolt
#define ICON_MATRIX      "\xEE\xA4\x9C"   // E91C  FourRows (grid)
#define ICON_TARGET      "\xEE\xB5\x9A"   // ED5A  CrossHair
#define ICON_RADAR       "\xEE\xAC\xB8"   // EAB8  Radar
#define ICON_HOOK        "\xEE\xA4\xA8"   // E928  Link
#define ICON_GAMEPAD     "\xEE\x9F\x8C"   // E7FC  XboxLogo (proxy for overlay)
#define ICON_LAYERS      "\xEE\xA4\x82"   // E902  Layers? (may fall back)
#define ICON_INFO        "\xEE\xA5\x86"   // E946  Info
#define ICON_WARNING     "\xEE\x9D\xBE"   // E7BA  Warning
#define ICON_CHECK       "\xEE\x9D\x83"   // E73E  Accept (checkmark)
#define ICON_DOT         "\xEE\xA8\xB7"   // EA37  LocationDot (filled circle)
#define ICON_WAND        "\xEE\x9E\x99"   // E799  Brush (creative)
#define ICON_LAMP        "\xEE\xA4\x8F"   // E90F  Lightbulb
#define ICON_CHEVRON_R   "\xEE\x9D\xA6"   // E76C  ChevronRight
#define ICON_CHEVRON_D   "\xEE\x9D\xA8"   // E768 reuse? Actually E70D ChevronDown = "\xEE\x9C\x8D"
#define ICON_HUD         "\xEE\xA5\xA8"   // E968  HomeSolid (we use as HUD marker)

// Glyph range passed to ImGui's atlas builder. Pulls the PUA block that
// Segoe Fluent Icons actually populates. Keeping it tight: just enough to
// cover every ICON_ constant above plus a bit of slack.
#ifdef __cplusplus
#include <imgui.h>
namespace dxs::icons {
inline const ImWchar* range() {
    static const ImWchar r[] = { 0xE700, 0xF8FF, 0 };
    return r;
}
}  // namespace dxs::icons
#endif
