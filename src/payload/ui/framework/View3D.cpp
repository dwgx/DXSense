#include "View3D.hpp"

#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <cmath>

namespace dxs::view3d {

namespace {

constexpr float kPi         = 3.14159265358979323846f;
constexpr float kFovY       = 55.0f * (kPi / 180.0f);
constexpr float kPitchLimit = 1.4f;

struct ProjectedPoint {
    ImVec2 xy{};
    float  depth = 0.0f;
    bool   visible = false;
};

float length(ImVec3 v) {
    return std::sqrt(dot(v, v));
}

ImVec3 safe_normalize(ImVec3 v, ImVec3 fallback) {
    const float len = length(v);
    if (len <= 1e-5f) {
        return fallback;
    }
    return v / len;
}

void update_camera(Canvas& c) {
    const OrbitState orbit = c.state ? *c.state : OrbitState{};

    const float cp = std::cos(orbit.pitch);
    const float sp = std::sin(orbit.pitch);
    const float cy = std::cos(orbit.yaw);
    const float sy = std::sin(orbit.yaw);

    c.camera_pos = orbit.center + orbit.dist * ImVec3{cp * cy, sp, cp * sy};

    const ImVec3 world_up{0.0f, 1.0f, 0.0f};
    c.view_back  = safe_normalize(c.camera_pos - orbit.center, ImVec3{0.0f, 0.0f, 1.0f});
    c.view_right = safe_normalize(cross(world_up, c.view_back), ImVec3{1.0f, 0.0f, 0.0f});
    c.view_up    = safe_normalize(cross(c.view_back, c.view_right), world_up);
    c.aspect     = std::max(1.0f, c.size.x) / std::max(1.0f, c.size.y);
}

ProjectedPoint project_world_to_canvas(const Canvas& c, ImVec3 world) {
    ProjectedPoint out{};

    const ImVec3 rel = world - c.camera_pos;
    const float  vx  = dot(rel, c.view_right);
    const float  vy  = dot(rel, c.view_up);
    const float  vz  = dot(rel, c.view_back);
    const float  w   = -vz;
    if (w <= 0.0f) {
        return out;
    }

    const float f = 1.0f / std::tan(kFovY * 0.5f);
    const float ndc_x = (vx * (f / c.aspect)) / w;
    const float ndc_y = (vy * f) / w;

    out.xy = {
        c.tl.x + (ndc_x * 0.5f + 0.5f) * c.size.x,
        c.tl.y + (0.5f - ndc_y * 0.5f) * c.size.y,
    };
    out.depth   = w;
    out.visible = true;
    return out;
}

void queue_line(Canvas& c, const ProjectedPoint& a, const ProjectedPoint& b,
                ImU32 color, float thickness_px) {
    if (!a.visible || !b.visible) {
        return;
    }
    c.lines.push_back(Canvas::QueuedLine{
        a.xy,
        b.xy,
        0.5f * (a.depth + b.depth),
        color,
        thickness_px,
    });
}

}  // namespace

ImVec3 operator+(ImVec3 a, ImVec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

ImVec3 operator-(ImVec3 a, ImVec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

ImVec3 operator*(ImVec3 v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

ImVec3 operator*(float s, ImVec3 v) {
    return v * s;
}

ImVec3 operator/(ImVec3 v, float s) {
    return {v.x / s, v.y / s, v.z / s};
}

float dot(ImVec3 a, ImVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

ImVec3 cross(ImVec3 a, ImVec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

ImVec3 normalize(ImVec3 v) {
    return safe_normalize(v, ImVec3{0.0f, 0.0f, 0.0f});
}

Canvas begin(const char* id, ImVec2 size, OrbitState* state) {
    if (size.x <= 0.0f) {
        size.x = ImGui::GetContentRegionAvail().x;
    }
    if (size.y <= 0.0f) {
        size.y = ImGui::GetContentRegionAvail().y;
    }
    size.x = std::max(size.x, 1.0f);
    size.y = std::max(size.y, 1.0f);

#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18900
    ImGui::InvisibleButton(
        id, size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
#else
    ImGui::InvisibleButton(id, size);
#endif

    Canvas c{};
    c.tl        = ImGui::GetItemRectMin();
    c.size      = ImGui::GetItemRectSize();
    c.state     = state;
    c.draw_list = ImGui::GetWindowDrawList();

    const ImVec2 br = c.tl + c.size;
    c.draw_list->AddRectFilled(
        c.tl, br, IM_COL32(21, 21, 21, 255), theme::radius_md);
    c.draw_list->AddRect(
        c.tl, br, theme::to_u32(theme::outline), theme::radius_md, 0, 1.0f);
    theme::draw_inner_highlight(c.tl, br, theme::radius_md);
    c.draw_list->PushClipRect(c.tl, br, true);

    if (state) {
        auto& io = ImGui::GetIO();
        const bool hovered = ImGui::IsItemHovered();
        const bool active  = ImGui::IsItemActive();
        if (ImGui::IsItemHovered() && std::fabs(io.MouseWheel) > 1e-4f) {
            state->dist = std::clamp(
                state->dist * std::pow(0.88f, io.MouseWheel), 2.0f, 400.0f);
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            state->yaw   += io.MouseDelta.x * 0.01f;
            state->pitch -= io.MouseDelta.y * 0.01f;
            state->pitch  = std::clamp(state->pitch, -kPitchLimit, kPitchLimit);
        }

        update_camera(c);

        if ((active || hovered) &&
            ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const ImVec3 pan =
                ((-io.MouseDelta.x) * c.view_right + io.MouseDelta.y * c.view_up) *
                (state->dist * 0.002f);
            state->center = state->center + pan;
        }
    }

    update_camera(c);
    return c;
}

void end(Canvas& c) {
    if (!c.draw_list) {
        return;
    }

    std::sort(c.lines.begin(), c.lines.end(),
              [](const Canvas::QueuedLine& a, const Canvas::QueuedLine& b) {
                  return a.depth > b.depth;
              });
    for (const auto& line : c.lines) {
        c.draw_list->AddLine(line.a, line.b, line.color, line.thickness_px);
    }
    c.draw_list->PopClipRect();
}

void draw_point(Canvas& c, ImVec3 pos, float radius_px, ImU32 color) {
    const ProjectedPoint p = project_world_to_canvas(c, pos);
    if (!p.visible) {
        return;
    }
    c.draw_list->AddCircleFilled(p.xy, radius_px, color, 16);
}

void draw_line(Canvas& c, ImVec3 a, ImVec3 b, ImU32 color, float thickness_px) {
    queue_line(c, project_world_to_canvas(c, a), project_world_to_canvas(c, b),
               color, thickness_px);
}

void draw_text(Canvas& c, ImVec3 pos, ImU32 color, const char* txt) {
    const ProjectedPoint p = project_world_to_canvas(c, pos);
    if (!p.visible || !txt) {
        return;
    }
    const ImVec2 sz = ImGui::CalcTextSize(txt);
    c.draw_list->AddText(
        ImVec2(p.xy.x + 5.0f, p.xy.y - sz.y - 2.0f), color, txt);
}

void draw_grid_xz(Canvas& c, float extent_m, float step_m, ImU32 major, ImU32 minor) {
    if (step_m <= 0.0f || extent_m <= 0.0f) {
        return;
    }

    for (float v = -extent_m; v <= extent_m + 0.001f; v += step_m) {
        const int   step_idx = static_cast<int>(std::round(std::fabs(v / step_m)));
        const ImU32 col      = (step_idx % 5 == 0) ? major : minor;
        draw_line(c, {v, 0.0f, -extent_m}, {v, 0.0f, extent_m}, col, 1.0f);
        draw_line(c, {-extent_m, 0.0f, v}, {extent_m, 0.0f, v}, col, 1.0f);
    }
}

void draw_axes(Canvas& c, ImVec3 origin, float length_m) {
    draw_line(c, origin, origin + ImVec3{length_m, 0.0f, 0.0f}, IM_COL32(235, 92, 92, 255), 2.0f);
    draw_line(c, origin, origin + ImVec3{0.0f, length_m, 0.0f}, IM_COL32(98, 218, 122, 255), 2.0f);
    draw_line(c, origin, origin + ImVec3{0.0f, 0.0f, length_m}, IM_COL32(90, 150, 255, 255), 2.0f);
    draw_text(c, origin + ImVec3{length_m, 0.0f, 0.0f}, IM_COL32(235, 92, 92, 255), "X");
    draw_text(c, origin + ImVec3{0.0f, length_m, 0.0f}, IM_COL32(98, 218, 122, 255), "Y");
    draw_text(c, origin + ImVec3{0.0f, 0.0f, length_m}, IM_COL32(90, 150, 255, 255), "Z");
}

void draw_frustum(Canvas& c, ImVec3 origin, ImVec3 forward,
                  ImVec3 up, float fov_y_rad, float aspect,
                  float near_m, float far_m, ImU32 color) {
    if (fov_y_rad <= 0.0f || aspect <= 0.0f || near_m <= 0.0f || far_m <= near_m) {
        return;
    }

    const ImVec3 fwd   = safe_normalize(forward, ImVec3{0.0f, 0.0f, -1.0f});
    const ImVec3 right = safe_normalize(cross(fwd, up), ImVec3{1.0f, 0.0f, 0.0f});
    const ImVec3 up_n  = safe_normalize(cross(right, fwd), ImVec3{0.0f, 1.0f, 0.0f});

    const float tan_half = std::tan(fov_y_rad * 0.5f);
    const float near_h   = tan_half * near_m;
    const float near_w   = near_h * aspect;
    const float far_h    = tan_half * far_m;
    const float far_w    = far_h * aspect;

    const ImVec3 near_c = origin + fwd * near_m;
    const ImVec3 far_c  = origin + fwd * far_m;

    const ImVec3 ntl = near_c + up_n * near_h - right * near_w;
    const ImVec3 ntr = near_c + up_n * near_h + right * near_w;
    const ImVec3 nbl = near_c - up_n * near_h - right * near_w;
    const ImVec3 nbr = near_c - up_n * near_h + right * near_w;
    const ImVec3 ftl = far_c + up_n * far_h - right * far_w;
    const ImVec3 ftr = far_c + up_n * far_h + right * far_w;
    const ImVec3 fbl = far_c - up_n * far_h - right * far_w;
    const ImVec3 fbr = far_c - up_n * far_h + right * far_w;

    draw_line(c, origin, ntl, color);
    draw_line(c, origin, ntr, color);
    draw_line(c, origin, nbl, color);
    draw_line(c, origin, nbr, color);

    draw_line(c, ntl, ntr, color);
    draw_line(c, ntr, nbr, color);
    draw_line(c, nbr, nbl, color);
    draw_line(c, nbl, ntl, color);

    draw_line(c, ftl, ftr, color);
    draw_line(c, ftr, fbr, color);
    draw_line(c, fbr, fbl, color);
    draw_line(c, fbl, ftl, color);

    draw_line(c, ntl, ftl, color);
    draw_line(c, ntr, ftr, color);
    draw_line(c, nbl, fbl, color);
    draw_line(c, nbr, fbr, color);
}

}  // namespace dxs::view3d
