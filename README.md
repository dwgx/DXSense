# DXSense

In-process debug overlay for Windows games. Injects a DLL that hooks the host renderer's swap chain and draws an ImGui panel inside the running process — no external window, no screen capture, zero external dependency at runtime beyond what the target already loads.

Built as a foundation for NeoX3 / DirectX 11 targets, with an abstraction layer to add DX12 or Vulkan backends later.

---

## Architecture

```
DXSense/
├─ src/
│  ├─ payload/                    # The injected DLL (DXSense.dll)
│  │  ├─ DllMain.cpp              # Entry — spawns boot thread, returns immediately
│  │  ├─ core/
│  │  │  ├─ Engine.{hpp,cpp}      # Lifecycle orchestrator (own/start/stop all subsystems)
│  │  │  └─ Logger.{hpp,cpp}      # File + OutputDebugString log, mutex-safe
│  │  ├─ hook/
│  │  │  ├─ HookManager.{hpp,cpp} # MinHook RAII wrapper — tracks and removes hooks on unload
│  │  │  └─ WndProcHook.{hpp,cpp} # GWLP_WNDPROC subclass (window-scoped, cleanly reversible)
│  │  ├─ render/
│  │  │  ├─ IRenderBackend.hpp    # Backend contract (install / uninstall)
│  │  │  └─ Dx11Backend.{hpp,cpp} # DX11: dummy-swapchain vtable scan, Present/ResizeBuffers hooks
│  │  └─ ui/
│  │     └─ Overlay.{hpp,cpp}     # ImGui draw loop + style config + INSERT toggle
│  └─ injector/
│     └─ Injector.cpp             # Standalone loader: VirtualAllocEx + CreateRemoteThread(LoadLibraryW)
└─ cmake/
   └─ Dependencies.cmake          # FetchContent: imgui v1.91.5-docking, MinHook v1.3.4
```

### Design decisions

| What | How | Why not the usual way |
|------|-----|-----------------------|
| Vtable acquisition | Dummy `D3D11CreateDeviceAndSwapChain` against a message-only HWND | kiero works but relies on hand-maintained vtable index tables that break across SDK updates |
| WndProc input | `SetWindowLongPtrW(GWLP_WNDPROC, ...)` subclass on the swap chain's HWND | Global `SetWindowsHookEx(WH_KEYBOARD_LL)` pollutes every window in the session |
| Hook library | MinHook — wraps with RAII `HookManager`, bulk remove on uninstall | Most overlays scatter raw `MH_*` calls; clean teardown requires centralization |
| CRT linkage | `/MT` static | Avoids MSVC runtime version collisions with whatever the host loaded |
| DllMain | Spawn thread → return immediately | Work under the loader lock causes deadlocks with DX11's internal initialisation |
| Backend abstraction | `IRenderBackend` → `Dx11Backend` | Swap in `Dx12Backend` / `VulkanBackend` without touching Engine or Overlay |

---

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or 2026 with **Desktop development with C++** workload
- CMake ≥ 3.24
- Git

---

## Build

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

Outputs land in `build\bin\Release\`:

| File | Role |
|------|------|
| `DXSense.dll` | Payload — the overlay |
| `dxs_inject.exe` | Loader — injects the DLL into a target process |

---

## Usage

```
dxs_inject <process.exe | PID> [path\to\DXSense.dll]
```

The DLL path is optional; when omitted the injector looks for `DXSense.dll` in its own directory.

```bat
dxs_inject.exe dwrg.exe
```

**INSERT** toggles overlay visibility. Log output goes to `DXSense.log` next to the host executable and is also visible in any debug output viewer (e.g. DebugView).

---

## Extending

**Add a debug panel** — create a class in `src/payload/ui/`, call its `draw()` from `Overlay::draw()`.

**Add a DX12 backend** — implement `IRenderBackend`, hook `ID3D12CommandQueue::ExecuteCommandLists` the same vtable-scan way. Register it in `Engine::start()`.

**Add a Vulkan backend** — same pattern; hook `vkQueuePresentKHR` via the instance dispatch table.

---

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| INSERT | Toggle overlay on / off |

---

## Dependencies

| Library | Version | License |
|---------|---------|---------|
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.91.5-docking | MIT |
| [MinHook](https://github.com/TsudaKageyu/minhook) | v1.3.4 | BSD 2-Clause |
