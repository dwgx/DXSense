add_library(dxsense SHARED
    DllMain.cpp
    core/Config.cpp
    core/Engine.cpp
    core/EventLog.cpp
    core/Keybinds.cpp
    core/Localization.cpp
    core/Logger.cpp
    core/RemoteBridge.cpp
    game/CameraSampler.cpp
    game/GameMemory.cpp
    hook/HookManager.cpp
    hook/WndProcHook.cpp
    render/Dx11Backend.cpp
    scripting/PythonBridge.cpp
    ui/Overlay.cpp
    ui/framework/ClickGui.cpp
    ui/framework/Fonts.cpp
    ui/framework/Splash.cpp
    ui/framework/Theme.cpp
    ui/hud/HudManager.cpp
    ui/hud/widgets/CrosshairWidget.cpp
    ui/hud/widgets/RadarWidget.cpp
    ui/hud/widgets/StatsWidget.cpp
    ui/panels/EntitiesPanel.cpp
    ui/panels/HooksPanel.cpp
    ui/panels/MatrixPanel.cpp
    ui/panels/ModulesPanel.cpp
    ui/panels/MemoryPanel.cpp
    ui/panels/OverviewPanel.cpp
    ui/panels/PythonReplPanel.cpp
    ui/panels/QuickActionsPanel.cpp
    ui/panels/RaycastPanel.cpp
    ui/panels/RpcTracerPanel.cpp
    ui/panels/SettingsPanel.cpp
)

set_target_properties(dxsense PROPERTIES
    OUTPUT_NAME  "DXSense"
    PREFIX       ""
)

target_include_directories(dxsense PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(dxsense PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(dxsense PRIVATE
    /W4 /permissive- /Zc:preprocessor /utf-8 /EHsc /MP
)

# Stage downloaded UI fonts next to the built DLL so the runtime can find
# them via GetModuleFileName + "/fonts/".
add_custom_command(TARGET dxsense POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:dxsense>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DXS_FONTS_DIR}/Inter.ttf"
            "$<TARGET_FILE_DIR:dxsense>/fonts/Inter.ttf"
    VERBATIM)

target_link_libraries(dxsense PRIVATE
    imgui
    minhook
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    shell32   # SHGetKnownFolderPath
    ole32     # CoTaskMemFree
    psapi     # GetModuleInformation
    ws2_32    # RemoteBridge (WSAStartup / socket / bind / listen / accept)
)
