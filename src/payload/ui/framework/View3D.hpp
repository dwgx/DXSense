#pragma once

#include <imgui.h>

#include <vector>

namespace dxs::view3d {

struct ImVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

ImVec3 operator+(ImVec3 a, ImVec3 b);
ImVec3 operator-(ImVec3 a, ImVec3 b);
ImVec3 operator*(ImVec3 v, float s);
ImVec3 operator*(float s, ImVec3 v);
ImVec3 operator/(ImVec3 v, float s);

float  dot(ImVec3 a, ImVec3 b);
ImVec3 cross(ImVec3 a, ImVec3 b);
ImVec3 normalize(ImVec3 v);

struct OrbitState {
    float  yaw   = 0.6f;
    float  pitch = 0.55f;
    float  dist  = 18.0f;
    ImVec3 center{0.0f, 0.0f, 0.0f};
};

struct Canvas {
    ImVec2      tl{};
    ImVec2      size{};
    OrbitState* state = nullptr;

    // Internal state for draw_* helpers. Callers should treat everything
    // below as opaque implementation detail.
    ImDrawList* draw_list = nullptr;
    ImVec3      camera_pos{};
    ImVec3      view_right{};
    ImVec3      view_up{};
    ImVec3      view_back{};
    float       aspect = 1.0f;

    struct QueuedLine {
        ImVec2 a{};
        ImVec2 b{};
        float  depth = 0.0f;
        ImU32  color = 0;
        float  thickness_px = 1.5f;
    };
    std::vector<QueuedLine> lines{};
};

Canvas begin(const char* id, ImVec2 size, OrbitState* state);
void   end(Canvas& c);

void draw_point(Canvas& c, ImVec3 pos, float radius_px, ImU32 color);
void draw_line(Canvas& c, ImVec3 a, ImVec3 b, ImU32 color, float thickness_px = 1.5f);
void draw_text(Canvas& c, ImVec3 pos, ImU32 color, const char* txt);

void draw_grid_xz(Canvas& c, float extent_m = 40.0f, float step_m = 2.0f,
                  ImU32 major = IM_COL32(255, 255, 255, 42),
                  ImU32 minor = IM_COL32(255, 255, 255, 16));
void draw_axes(Canvas& c, ImVec3 origin = {0.0f, 0.0f, 0.0f}, float length_m = 2.0f);
void draw_frustum(Canvas& c, ImVec3 origin, ImVec3 forward,
                  ImVec3 up, float fov_y_rad, float aspect,
                  float near_m, float far_m, ImU32 color);

}  // namespace dxs::view3d
