#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct ImVec2;

namespace dxs {

struct Vec3 { float x{}, y{}, z{}; };

// Per-frame camera state sampled from the engine's Python layer.
//
// Design:
//   * One Python exec per sample tick (20 Hz by default). The tick is
//     driven from the overlay's per-frame on_frame() — we deliberately do
//     NOT run on our own thread, because the host's Python threads already
//     contend for the GIL; a worker thread here would just queue behind
//     them and gain nothing.
//   * Widgets never call the bridge themselves. They register requested
//     world->screen points via request_world_to_screen(), then read the
//     result from the snapshot on the next frame. This consolidates all
//     camera-related Python work into one call per tick regardless of
//     widget count.
//   * A Python-side helper module `_dxs_cam` is installed lazily on first
//     sample; it caches CameraCtrl / WorldBattle refs inside the
//     interpreter so we don't walk gc.get_objects() every tick.
class CameraSampler {
public:
    static CameraSampler& instance();

    struct Snapshot {
        bool                  camera_ready = false;
        std::array<float, 16> view{};          // row-major, as printed by math3d.matrix.get(r,c)
        std::array<float, 16> proj{};
        std::array<float, 16> view_proj{};     // precomputed view * proj for convenience
        Vec3                  cam_pos{};
        Vec3                  cam_forward{};
        Vec3                  cam_right{};
        Vec3                  cam_up{};
        float                 fov_y = 0.0f;

        bool                  player_ready = false;
        std::uint64_t         player_uid = 0;
        Vec3                  player_pos{};

        // World / scene identity from WorldManager.cur_world.create_info.
        // world_type == 0 is the main city-hall lobby; anything in the
        // MATCH_WORLD_LIST (see project_neox3_internals.md) is a match.
        int                   world_type = -1;
        int                   scene_id   = 0;
        std::string           world_class;   // class name of cur_world
        bool                  in_battle  = false;  // LagMgr.in_battle

        // Everything alive in the current match. Populated every tick from
        // UnitManager.units_by_type so radar / raycast don't have to depend
        // on the user having opened the EntitiesPanel.
        struct Unit {
            std::uint64_t uid  = 0;
            int           kind = 0;   // 1=BUTCHER 2=CIVILIAN 3=GENERATOR 4=HOOK
                                      // 5=BOX 6=DOOR 7=WOOD 8=PANEL 9=CUPBOARD
                                      // 10=CAVE 11=CROW 12=SWITCH 510=SPIRIT
            Vec3          pos{};
        };
        std::vector<Unit>     units;

        // Keyed by whatever uid the caller supplied to request_world_to_screen.
        std::unordered_map<std::uint64_t, std::pair<float, float>> screen;

        double                sample_time   = 0.0;  // ImGui::GetTime() at sample
        int                   sample_count  = 0;    // monotonic
        double                last_duration = 0.0;  // seconds blocked in Python
        std::string           last_error;           // empty if last tick was clean
    };

    // Cheap snapshot copy. Call from render thread.
    Snapshot snapshot() const;

    // Register world positions the widgets want projected on the NEXT tick.
    // Replaces the current request set — callers should re-submit their
    // full set each frame (this is what widgets naturally do anyway).
    void request_world_to_screen(std::vector<std::pair<std::uint64_t, Vec3>> pts);

    // Drives the throttled sample tick. Safe to call every frame.
    void on_frame();

    // Knobs.
    void  set_rate_hz(float hz);
    float rate_hz() const;
    void  set_enabled(bool on);
    bool  enabled() const;

private:
    CameraSampler() = default;
    void sample_now();
    bool parse_output(const std::string& text, Snapshot& out) const;

    void diff_and_emit_events(const Snapshot& prev, const Snapshot& next);

    mutable std::mutex                                    mtx_;
    Snapshot                                              latest_{};
    std::vector<std::pair<std::uint64_t, Vec3>>           request_;
    bool                                                  helper_installed_ = false;
    bool                                                  enabled_ = true;
    double                                                next_tick_at_ = 0.0;
    double                                                interval_ = 1.0 / 20.0;

    // Event emission throttle — heartbeat positions emit at ~1 Hz even
    // though the sampler itself ticks at 20 Hz.
    double                                                next_heartbeat_at_ = 0.0;
    double                                                heartbeat_interval_ = 1.0;
};

}  // namespace dxs
