# DXSense

> **状态：项目已终止（Project Terminated）。仓库仅作存档。**
> **Status: TERMINATED. This repository is preserved as an archive only.**

In-process debug overlay framework for Windows games — DLL injection, ImGui draw layer, MinHook trampolines, abstracted render backend (DX11 implemented), embedded Python bridge for live debug.

[**▶ Live UI Demo (GitHub Pages)**](https://dwgx.github.io/DXSense/) — 静态 HTML，纯 UI 展示，**不含任何运行时代码 / 注入器 / 工程实现**。

---

## ⚠️ 使用条款 / Terms of Use

**本仓库仅供学习与个人技术研究使用。**
**This repository is published for personal study and technical research only.**

### 严格禁止 / Strictly Prohibited

* **禁止用于任何在线 / 竞技 / 多人游戏环境**。任何用以对抗第三方反作弊系统的行为与本仓库作者无关。
  Use against any online, competitive, or multiplayer game's anti-cheat system is **strictly forbidden**.

* **禁止商业使用**：不得销售、出租、订阅、SaaS 化、嵌入付费产品、有偿教学。
  No commercial use of any kind. No selling, renting, subscriptions, SaaS, paid tutorials, or bundling into paid products.

* **禁止二次分发与镜像**：不得再上传到其他公开平台、分发渠道、Telegram 频道、QQ 群、Discord 服务器、网盘、论坛、cheat 网站等。仅允许通过本仓库 URL 引用。
  No redistribution. Do not re-upload to other public platforms, mirrors, file shares, IM channels, forums, or cheat aggregators. Reference by this repository's URL only.

* **禁止打包发布二进制**：不得编译为成品 `.exe` / `.dll` 后分发或上传。
  No binary distribution. Do not compile and distribute or share built artifacts.

* **禁止在视频 / 直播中宣传作弊用途**。引擎逆向、DLL 注入、Hook、ImGui 集成的纯技术讲解可以；**宣传或暗示对在线游戏使用一律禁止**。
  No promotion of cheating in video / streaming content. Engineering tutorials are OK; cheat promotion is not.

* **禁止用于规避正版软件保护**：不得用本仓库的代码绕过任何商业软件 / 游戏 / DRM 的反作弊或反破解系统。
  No use for bypassing legitimate software protection / DRM / anti-cheat.

* **禁止移除版权与免责声明**：分支、Fork、引用本仓库代码必须保留本 README 完整条款。
  Forks and derivatives must retain this README in full.

### 仅允许 / Permitted Only

* 阅读源码、学习架构与实现思路
  Reading source for architecture / implementation study
* 在个人离线 / 自有 demo 环境中编译、调试、改写
  Building and modifying for personal offline experimentation on software you own
* 引用本仓库的设计模式（DLL 注入、Hook 生命周期、ImGui 集成、Procedure Fabric）到你自己的工程，**前提是你的工程同样不违反上述任何禁止条款**
  Borrowing patterns into your own project, **provided your project also does not violate any of the above prohibitions**

---

## ❗ 免责声明 / Disclaimer

* 本工程**仅作为 Windows DLL 注入 / ImGui overlay / 渲染后端 hook / 嵌入 Python 桥接的技术演示存档**。本仓库的开发者**不对任何下游使用承担责任**。
  This is a technical archive of injection / overlay / hook / Python-bridge patterns. The author bears no responsibility for downstream use.

* 任何因违反上述条款导致的封号、法律责任、合规问题、纠纷，**完全由使用者自行承担**。
  Any account ban, legal liability, compliance issue, or dispute arising from violating the above terms is **entirely the user's responsibility**.

* 本仓库**不提供**：编译好的 DLL、注入器二进制、用户支持、问题答复。Issue 区不接受任何"怎么用 / 为什么不工作 / 我被 ban 了"类问题，相关 issue 一律关闭。
  This repo provides **no**: built DLLs / injector binaries / user support / how-to answers. "How do I use this" / "Why doesn't it work" / "I got banned" issues will be closed without response.

* 项目已于 **2026-04** 终止开发。**不会**接受功能 PR、不维护、不修 bug、不出新版本。Issue / PR 一律不予处理。
  Development ceased in April 2026. **No** feature PRs, maintenance, bugfixes, or new releases. Issues and PRs will not be processed.

---

## Architecture

```
DXSense/
├─ docs/                          # GitHub Pages — static UI design preview
├─ src/
│  ├─ payload/                    # The injected DLL (DXSense.dll)
│  │  ├─ DllMain.cpp              # Entry — spawns boot thread, returns immediately
│  │  ├─ core/
│  │  │  ├─ Engine.{hpp,cpp}      # Lifecycle orchestrator
│  │  │  ├─ Logger.{hpp,cpp}      # Mutex-safe log
│  │  │  └─ procedure/            # "Procedure Fabric" — Loom / Tick / Pin / Phase / Intent
│  │  ├─ hook/
│  │  │  ├─ HookManager.{hpp,cpp} # MinHook RAII — bulk remove on unload
│  │  │  └─ WndProcHook.{hpp,cpp} # GWLP_WNDPROC subclass
│  │  ├─ render/
│  │  │  ├─ IRenderBackend.hpp    # Backend contract
│  │  │  └─ Dx11Backend.{hpp,cpp} # DX11: dummy-swapchain vtable scan, Present / ResizeBuffers hooks
│  │  ├─ scripting/               # Embedded Python bridge — live peek / poke / call
│  │  └─ ui/                      # ImGui panels + framework
│  └─ injector/
│     └─ Injector.cpp             # VirtualAllocEx + CreateRemoteThread(LoadLibraryW)
└─ cmake/
   └─ Dependencies.cmake          # FetchContent: imgui v1.91.5-docking, MinHook v1.3.4
```

### Notable design choices

| What | How | Why not the usual way |
|------|-----|-----------------------|
| Vtable acquisition | Dummy `D3D11CreateDeviceAndSwapChain` against message-only HWND | Hand-maintained vtable index tables break across SDK updates |
| WndProc input | `SetWindowLongPtrW` subclass on swap-chain HWND | Global `SetWindowsHookEx` pollutes every window in the session |
| Hook lifecycle | MinHook + RAII `HookManager`, bulk remove on uninstall | Scattered raw `MH_*` calls leak hooks on teardown |
| CRT linkage | `/MT` static | Avoids MSVC runtime version collisions with the host process |
| DllMain | Spawn thread → return immediately | Work under the loader lock deadlocks DX11 init |
| Procedure Fabric | Loom + typed Pins + Tick stream | Long-lived behaviors need declarative engage/disengage, not ad-hoc threads |

---

## Requirements

* Windows 10/11 x64
* Visual Studio 2022 or 2026 with **Desktop development with C++** workload
* CMake ≥ 3.24
* Git

## Build

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

Outputs land in `build\bin\Release\`:

| File | Role |
|------|------|
| `DXSense.dll` | Payload — the overlay |
| `dxs_inject.exe` | Loader |

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

These dependencies' upstream licenses **only cover their own code**, not this repository.

---

## License

**All Rights Reserved** — see "Terms of Use" above.

This is **not** an open-source license. Public visibility on GitHub ≠ permission to redistribute / modify / use commercially. If you want similar functionality with permissive licensing, write your own from scratch — the patterns are widely documented elsewhere (kiero / GuidedHacking / MinHook upstream docs).

Copyright (C) 2026 dwgx
