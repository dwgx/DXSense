#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

struct ImVec2;

namespace dxs {

struct Vec3 { float x{}, y{}, z{}; };

// Per-frame camera state sampled from the engine's Python layer.
//
// Design:
//   * One Python exec per sample tick (15 Hz by default), driven from a
//     dedicated worker thread. The worker acquires the GIL only for the
//     duration of a snap() call, so the render thread never blocks on
//     Python.
//   * A Python-side helper module `_dxs_cam` is installed lazily on first
//     sample; it caches CameraCtrl / WorldManager / LagMgr / Avatar /
//     UnitManager refs inside the interpreter so we don't walk
//     gc.get_objects() every tick.
//   * The helper prints a compact text protocol back through
//     PythonBridge::exec_and_collect; the matrices it ships are the REAL
//     view (world-to-camera) and projection matrices, so widgets can
//     project world points on the C++ side without any engine round-trip.
//     See Snapshot::project() and scripts/verify_matrix.py for the proof.
//   * Per-uid velocity is EMA-smoothed across ticks so widgets can
//     dead-reckon positions between sampler ticks
//     (Snapshot::predict_position()).
class CameraSampler {
public:
    static CameraSampler& instance();

    struct Snapshot {
        bool                  camera_ready = false;
        // Row-major. `view` is world-to-camera (the true view matrix, obtained
        // via cam.transformation.inverse() on the Python side). `proj` is
        // cam.projection_matrix. `view_proj = view * proj` — suitable for
        // world-space → clip-space multiplication on the render thread.
        std::array<float, 16> view{};
        std::array<float, 16> proj{};
        std::array<float, 16> view_proj{};
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
            // EMA-smoothed world-space velocity (units/sec). Populated from
            // successive ticks by the sampler — widgets can extrapolate with
            // pos + vel * age_seconds() for dead-reckoning between ticks.
            Vec3          vel{};
        };
        std::vector<Unit>     units;

        double                sample_time   = 0.0;  // CameraSampler::now() at sample
        int                   sample_count  = 0;    // monotonic
        double                last_duration = 0.0;  // seconds blocked in Python
        std::string           last_error;           // empty if last tick was clean

        // Seconds since this snapshot was produced, on the monotonic clock
        // exposed by CameraSampler::now(). 0 before the first sample.
        double age_seconds() const;

        // World-space → pixel coordinates using view_proj and the supplied
        // viewport. Returns nullopt if the point is behind the camera
        // (clip.w <= 0) or the snapshot has no valid matrices yet.
        //
        // Match for NeoX3/dwrg verified in scripts/verify_matrix.py: engine
        // uses DX-style Y-down screen, so we remap ndc_y via (1 - ndc_y)/2.
        std::optional<std::pair<float, float>> project(
            Vec3 world, float viewport_w, float viewport_h) const;

        // Dead-reckon a unit's position to `at_time` on CameraSampler::now()
        // axis, using its recorded EMA velocity. Falls back to the raw
        // position when `at_time` is <= sample_time or the velocity is zero.
        Vec3 predict_position(const Unit& u, double at_time) const;
    };

    // Cheap snapshot copy. Call from render thread.
    Snapshot snapshot() const;

    // "Now" on the same steady-clock axis as Snapshot::sample_time. Render
    // thread callers use this to compute sample age without the worker's
    // ImGui-free clock diverging from ImGui::GetTime().
    static double now();

    // DEPRECATED. Previously queued points for engine-side projection that
    // completed one tick later; now C++ projects directly from view_proj
    // in the same frame via Snapshot::project(). Retained as a no-op so
    // downstream code compiles during the transition.
    [[deprecated("use Snapshot::project() instead")]]
    void request_world_to_screen(std::vector<std::pair<std::uint64_t, Vec3>>) {}

    // Lifecycle — start the background sampling thread. Render thread
    // must never block on Python execution, so the sampler runs on its
    // own worker that acquires the GIL and publishes Snapshots atomically.
    void start();
    void stop();

    // Deprecated no-op. Retained so existing Overlay::draw call sites keep
    // compiling during the transition.
    void on_frame() {}

    // Knobs.
    void  set_rate_hz(float hz);
    float rate_hz() const;
    void  set_enabled(bool on);
    bool  enabled() const;

private:
    CameraSampler() = default;
    void sample_now();
    void diff_and_emit_events(const Snapshot& prev, const Snapshot& next);
    void worker_loop();

    mutable std::mutex                                    mtx_;
    Snapshot                                              latest_{};
    bool                                                  helper_installed_ = false;

    // Persistent per-uid state used to compute velocity across ticks. Kept
    // out of Snapshot because we rewrite Snapshot wholesale each sample; the
    // sampler patches vel into each new snapshot's units from this table.
    struct UnitState {
        Vec3   pos{};
        Vec3   vel{};
        double t = 0.0;
    };
    std::unordered_map<std::uint64_t, UnitState>          unit_state_;
    std::atomic<bool>                                     enabled_{true};
    std::atomic<double>                                   interval_{1.0 / 30.0};

    // Worker thread + shutdown signalling.
    std::thread                                           worker_;
    std::atomic<bool>                                     worker_stop_{false};
    std::mutex                                            wake_mtx_;
    std::condition_variable                               wake_cv_;

    // Event emission throttle — heartbeat positions emit at ~1 Hz even
    // though the sampler itself ticks at 15 Hz by default.
    double                                                next_heartbeat_at_ = 0.0;
    double                                                heartbeat_interval_ = 1.0;
};

}  // namespace dxs
