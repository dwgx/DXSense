add_library(dxsense SHARED
    DllMain.cpp
    core/Logger.cpp
    core/Engine.cpp
    hook/HookManager.cpp
    hook/WndProcHook.cpp
    render/Dx11Backend.cpp
    scripting/PythonBridge.cpp
    ui/Overlay.cpp
    ui/framework/ClickGui.cpp
    ui/framework/Theme.cpp
    ui/panels/EntitiesPanel.cpp
    ui/panels/HooksPanel.cpp
    ui/panels/MatrixPanel.cpp
    ui/panels/MemoryPanel.cpp
    ui/panels/OverviewPanel.cpp
    ui/panels/PythonReplPanel.cpp
    ui/panels/QuickActionsPanel.cpp
    ui/panels/RaycastPanel.cpp
    ui/panels/RpcTracerPanel.cpp
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

target_link_libraries(dxsense PRIVATE
    imgui
    minhook
    d3d11
    dxgi
    d3dcompiler
    dwmapi
)
