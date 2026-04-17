include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---- UI fonts --------------------------------------------------------------
# Inter is downloaded once per clean build. Users reject the stock Windows
# font stack; Inter is the de-facto modern UI sans and survives scale well.
# Additional CJK / icon faces come from the Windows system fonts at runtime
# (DengXian Light, Segoe Fluent Icons).
set(DXS_FONTS_DIR "${CMAKE_BINARY_DIR}/bin/fonts")
file(MAKE_DIRECTORY "${DXS_FONTS_DIR}")

function(dxs_fetch_font url dest)
    if(NOT EXISTS "${dest}")
        message(STATUS "DXSense fonts: fetching ${url}")
        file(DOWNLOAD "${url}" "${dest}"
             TIMEOUT 60 INACTIVITY_TIMEOUT 30
             STATUS _status)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            list(GET _status 1 _err)
            message(WARNING "DXSense fonts: download failed (${_err})")
            file(REMOVE "${dest}")
        endif()
    endif()
endfunction()

dxs_fetch_font(
    "https://github.com/rsms/inter/raw/master/docs/font-files/InterVariable.ttf"
    "${DXS_FONTS_DIR}/Inter.ttf")


# ---- ImGui (docking branch) ------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5-docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# ImGui ships no CMakeLists; we wrap the exact sources we need (core + two backends).
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DEFINE_MATH_OPERATORS
)

# ---- MinHook ---------------------------------------------------------------
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(minhook)
