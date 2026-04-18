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

// Rendered as part of the overlay layer, ABOVE the ClickGui window but
// BELOW the splash. No-op when the palette is closed.
void command_palette_draw();

}  // namespace dxs
