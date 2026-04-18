#pragma once

namespace dxs::splash {

// One-shot boot splash drawn over everything else for ~1.4 s when the DLL
// first attaches. Self-contained — call begin() once from Engine::start
// after PythonBridge is up, then draw() every frame. It fades out and
// returns false once done; after that draw() is a no-op.

void begin();
void begin_exit(); // Trigger the cinematic fade-out sequence for uninject
bool active();
bool is_exit();    // Check if the current splash is the exit sequence
void draw();

}  // namespace dxs::splash
