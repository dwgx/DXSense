add_executable(dxs_inject Injector.cpp)

set_target_properties(dxs_inject PROPERTIES
    OUTPUT_NAME "dxs_inject"
)

target_compile_definitions(dxs_inject PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(dxs_inject PRIVATE /W4 /permissive- /utf-8 /EHsc)

target_link_libraries(dxs_inject PRIVATE
    advapi32
)
