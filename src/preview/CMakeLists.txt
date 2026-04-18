add_executable(dxs_preview WIN32
    main.cpp
    ../payload/core/Config.cpp
    ../payload/core/EventLog.cpp
    ../payload/core/Keybinds.cpp
    ../payload/core/Localization.cpp
    ../payload/core/Logger.cpp
    ../payload/core/RemoteBridge.cpp
    ../payload/core/procedure/Loom.cpp
    ../payload/core/procedure/Pin.cpp
    ../payload/core/procedure/Tick.cpp
    ../payload/core/procedure/procedures/AutoDecode.cpp
    ../payload/core/procedure/procedures/SpeedOverride.cpp
    ../payload/game/CameraSampler.cpp
    ../payload/game/GameMemory.cpp
    ../payload/hook/HookManager.cpp
    ../payload/scripting/PythonBridge.cpp
    ../payload/ui/Overlay.cpp
    ../payload/ui/framework/ClickGui.cpp
    ../payload/ui/framework/Fonts.cpp
    ../payload/ui/framework/Splash.cpp
    ../payload/ui/framework/Theme.cpp
    ../payload/ui/framework/View3D.cpp
    ../payload/ui/hud/HudManager.cpp
    ../payload/ui/hud/widgets/CrosshairWidget.cpp
    ../payload/ui/hud/widgets/EspWidget.cpp
    ../payload/ui/hud/widgets/RadarWidget.cpp
    ../payload/ui/hud/widgets/StatsWidget.cpp
    ../payload/ui/panels/AcObservatoryPanel.cpp
    ../payload/ui/panels/EntitiesPanel.cpp
    ../payload/ui/panels/HooksPanel.cpp
    ../payload/ui/panels/HudPanel.cpp
    ../payload/ui/panels/InteractionFatherPanel.cpp
    ../payload/ui/panels/MatrixPanel.cpp
    ../payload/ui/panels/MemoryPanel.cpp
    ../payload/ui/panels/ModulesPanel.cpp
    ../payload/ui/panels/OverviewPanel.cpp
    ../payload/ui/panels/PythonReplPanel.cpp
    ../payload/ui/panels/QuickActionsPanel.cpp
    ../payload/ui/panels/RaycastPanel.cpp
    ../payload/ui/panels/RpcTracerPanel.cpp
    ../payload/ui/panels/SettingsPanel.cpp
    ../payload/ui/panels/VelocityLabPanel.cpp
    ../payload/ui/panels/VulnLabPanel.cpp
)

target_include_directories(dxs_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../payload)

target_compile_definitions(dxs_preview PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE _UNICODE
    _CRT_SECURE_NO_WARNINGS
    DXS_PREVIEW
)

target_compile_options(dxs_preview PRIVATE
    /W4 /permissive- /Zc:preprocessor /utf-8 /EHsc /MP
)

target_link_libraries(dxs_preview PRIVATE
    imgui
    minhook
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    shell32   # SHGetKnownFolderPath
    ole32     # CoTaskMemFree
    psapi     # GetModuleInformation
    ws2_32    # RemoteBridge
)
