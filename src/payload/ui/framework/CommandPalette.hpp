#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  Command Palette — the Ctrl+K surface for finding panels without
//  navigating the sidebar. Ported from the v3 design.
//
//  One entry point, one exit point, one render call. Intentionally narrow
//  API because the palette is a singleton overlay with no per-instance
//  state worth exposing.
// ═════════════════════════════════════════════════════════════════════════

namespace dxs {

// Open the palette. Idempotent — calling while already open is a no-op
// (the palette focuses its input field on every open-transition anyway).
void open_command_palette();
void close_command_palette();
bool command_palette_open();

// Called from WndProcHook / Overlay input path so Ctrl+K toggles the
// palette regardless of which ImGui widget has focus.
void command_palette_on_key(int vk, bool ctrl_down);

// No-op kept for binary compatibility — the palette used to render as a
// centered modal on top of the ClickGui. It now renders INSIDE the
// ClickGui content area (see command_palette_draw_inline). Called from
// Overlay::draw as a stub so existing call sites compile unchanged.
void command_palette_draw();

// Renders the palette in place of the active panel inside the ClickGui
// content card. Called from ClickGui::draw_content when the palette is
// open. Inline rendering means the content area itself "becomes" the
// palette — no scrim, no extra window — so Ctrl+K feels like navigating
// to a new sidebar page rather than opening a modal.
void command_palette_draw_inline();

// True while the palette is animating — either opening (alpha rising)
// or closing (alpha falling but not yet 0). ClickGui uses this to know
// whether to keep routing the content card to the palette even after
// the user has pressed Esc, so the fade-out gets drawn before the
// active panel pops back.
bool command_palette_is_drawing();

}  // namespace dxs
