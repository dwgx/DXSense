#pragma once

// Material Design Icons webfont — codepoints mirrored from
// https://cdn.jsdelivr.net/npm/@mdi/font/css/materialdesignicons.min.css
// (package version 7.4.47 at fetch time).
//
// Each macro below is the UTF-8 encoding of one MDI codepoint, ready for
// concatenation with plain label text ("ICON " label).

#define MDI_VIEW_DASHBOARD_OUTLINE   "\xF3\xB0\xA8\x9D"   // U+F0A1D mdi-view-dashboard-outline
#define MDI_PUZZLE_OUTLINE           "\xF3\xB0\xA9\xA6"   // U+F0A66 mdi-puzzle-outline
#define MDI_MONITOR_DASHBOARD        "\xF3\xB0\xA8\x87"   // U+F0A07 mdi-monitor-dashboard
#define MDI_FILE_DOCUMENT_OUTLINE    "\xF3\xB0\xA7\xAE"   // U+F09EE mdi-file-document-outline
#define MDI_HAMMER_WRENCH            "\xF3\xB1\x8C\xA3"   // U+F1323 mdi-hammer-wrench
#define MDI_ACCOUNT_GROUP_OUTLINE    "\xF3\xB0\xAD\x98"   // U+F0B58 mdi-account-group-outline
#define MDI_GRID                     "\xF3\xB0\x8B\x81"   // U+F02C1 mdi-grid
#define MDI_RAY_START_ARROW          "\xF3\xB0\x91\x83"   // U+F0443 mdi-ray-start-arrow
#define MDI_SWAP_HORIZONTAL          "\xF3\xB0\x93\xA1"   // U+F04E1 mdi-swap-horizontal
#define MDI_SHIELD_SEARCH            "\xF3\xB0\xB6\x9A"   // U+F0D9A mdi-shield-search
#define MDI_MEMORY                   "\xF3\xB0\x8D\x9B"   // U+F035B mdi-memory
#define MDI_HOOK                     "\xF3\xB0\x9B\xA2"   // U+F06E2 mdi-hook
#define MDI_CONSOLE                  "\xF3\xB0\x86\x8D"   // U+F018D mdi-console
#define MDI_LIGHTNING_BOLT           "\xF3\xB1\x90\x8B"   // U+F140B mdi-lightning-bolt
#define MDI_FLASK_OUTLINE            "\xF3\xB0\x82\x96"   // U+F0096 mdi-flask-outline
#define MDI_SPEEDOMETER              "\xF3\xB0\x93\x85"   // U+F04C5 mdi-speedometer
#define MDI_GESTURE_TAP              "\xF3\xB0\x9D\x81"   // U+F0741 mdi-gesture-tap
#define MDI_COG_OUTLINE              "\xF3\xB0\xA2\xBB"   // U+F08BB mdi-cog-outline
#define MDI_MAGNIFY                  "\xF3\xB0\x8D\x89"   // U+F0349 mdi-magnify
#define MDI_CHECK                    "\xF3\xB0\x84\xAC"   // U+F012C mdi-check
#define MDI_CLOSE                    "\xF3\xB0\x85\x96"   // U+F0156 mdi-close
#define MDI_CHEVRON_RIGHT            "\xF3\xB0\x85\x82"   // U+F0142 mdi-chevron-right
#define MDI_CHEVRON_DOWN             "\xF3\xB0\x85\x80"   // U+F0140 mdi-chevron-down
#define MDI_CHEVRON_UP               "\xF3\xB0\x85\x83"   // U+F0143 mdi-chevron-up
#define MDI_CHEVRON_LEFT             "\xF3\xB0\x85\x81"   // U+F0141 mdi-chevron-left
#define MDI_DOTS_HORIZONTAL          "\xF3\xB0\x87\x98"   // U+F01D8 mdi-dots-horizontal
#define MDI_FOLDER_OUTLINE           "\xF3\xB0\x89\x96"   // U+F0256 mdi-folder-outline
#define MDI_PLUS                     "\xF3\xB0\x90\x95"   // U+F0415 mdi-plus
#define MDI_MINUS                    "\xF3\xB0\x8D\xB4"   // U+F0374 mdi-minus
#define MDI_REFRESH                  "\xF3\xB0\x91\x90"   // U+F0450 mdi-refresh
#define MDI_KEYBOARD                 "\xF3\xB0\x8C\x8C"   // U+F030C mdi-keyboard
#define MDI_INFORMATION_OUTLINE      "\xF3\xB0\x8B\xBD"   // U+F02FD mdi-information-outline
#define MDI_ALERT_OUTLINE            "\xF3\xB0\x80\xAA"   // U+F002A mdi-alert-outline
#define MDI_CHECK_CIRCLE_OUTLINE     "\xF3\xB0\x97\xA1"   // U+F05E1 mdi-check-circle-outline
#define MDI_PLAY                     "\xF3\xB0\x90\x8A"   // U+F040A mdi-play
#define MDI_PAUSE                    "\xF3\xB0\x8F\xA4"   // U+F03E4 mdi-pause

#ifdef __cplusplus
#include <imgui.h>
namespace dxs::mdi {
inline const ImWchar* range() {
    static const ImWchar r[] = { 0xF002A, 0xF140B, 0 };
    return r;
}
}  // namespace dxs::mdi
#endif
