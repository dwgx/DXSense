#include "EspVisual.hpp"

#include "core/Config.hpp"
#include "core/procedure/Tick.hpp"
#include "ui/hud/HudManager.hpp"
#include "ui/hud/widgets/EspWidget.hpp"

namespace dxs::procedure {

EspVisual::EspVisual()
    : identity_{
          .handle   = "esp_visual",
          .title    = "ESP Visuals",
          .domain   = Domain::Visual,
          .synopsis = "World-space unit overlay — kinds, markers, tracers.",
      },
      // Kinds.
      show_hunters_    (this, "hunters",      "Show hunters",   true),
      show_survivors_  (this, "survivors",    "Show survivors", true),
      show_props_      (this, "props",        "Show props",     false),
      // Marker composition.
      show_distance_   (this, "distance",     "Show distance",     true),
      show_box_        (this, "box",          "Screen-space box",  true),
      show_silhouette_ (this, "silhouette",   "Silhouette fill",   false),
      show_tracer_     (this, "tracer",       "Tracer line",       true),
      show_dot_        (this, "dot",          "Centre dot",        false),
      hunter_proximity_(this, "hunter_proxim","Hunter proximity cue", true),
      tracer_origin_   (this, "tracer_origin","Tracer origin",
                        { "Bottom", "Top", "Centre", "Crosshair" }, 0),
      silhouette_alpha_(this, "sil_alpha",    "Silhouette opacity", 0.55f,
                        {0.0f, 1.0f}),
      // 180 m matches the widget default; past that tracers overlap too
      // densely to be useful.
      max_distance_    (this, "max_distance", "Max distance (m)",   180.0f,
                        {10.0f, 500.0f})
{}

namespace {

// Flush every pin into the widget's config keys. The widget reads these
// prefix/`hud.esp.*` keys directly on construct AND re-reads them each
// frame through its own state — so writing here makes live edits visible
// without a reinject.
//
// NOTE: widget uses SHORT keys (`hud.esp.hunters`, `hud.esp.max_dist`),
// NOT the longer `.show_hunters` / `.max_distance` I had in the first
// pass of this procedure. Those don't match the widget's ctor and would
// silently no-op. Corrected to the widget's real key set.
void flush_to_widget(bool hunters, bool survivors, bool props,
                     bool distance, bool box, bool silhouette,
                     bool tracer, bool dot, bool hunter_prox,
                     int origin, float sil_alpha, float max_dist) {
    auto& cfg = Config::instance();
    cfg.set_bool ("hud.esp.hunters",       hunters);
    cfg.set_bool ("hud.esp.survivors",     survivors);
    cfg.set_bool ("hud.esp.props",         props);
    cfg.set_bool ("hud.esp.distance",      distance);
    cfg.set_bool ("hud.esp.box",           box);
    cfg.set_bool ("hud.esp.silhouette",    silhouette);
    cfg.set_bool ("hud.esp.tracer",        tracer);
    cfg.set_bool ("hud.esp.dot",           dot);
    cfg.set_bool ("hud.esp.hunter_proxim", hunter_prox);
    cfg.set_int  ("hud.esp.tracer_origin", origin);
    cfg.set_float("hud.esp.sil_alpha",     sil_alpha);
    cfg.set_float("hud.esp.max_dist",      max_dist);

    // Ping the widget to re-read its cached copy — Config writes alone
    // don't update the widget's member state.
    for (IHudWidget* w : HudManager::instance().all()) {
        if (w && w->id() == "esp") {
            static_cast<EspWidget*>(w)->rehydrate();
            break;
        }
    }
}

}  // namespace

void EspVisual::on_engage() {
    HudManager::instance().set_enabled("esp", true);
    flush_to_widget(show_hunters_, show_survivors_, show_props_,
                    show_distance_, show_box_, show_silhouette_,
                    show_tracer_, show_dot_, hunter_proximity_,
                    tracer_origin_, silhouette_alpha_, max_distance_);
}

void EspVisual::on_disengage() {
    HudManager::instance().set_enabled("esp", false);
}

Phase EspVisual::weave(Tick& t) {
    // Mirror pin edits into the widget's config keys every frame. Config
    // deduplicates same-value writes, so this is cheap even at 60 Hz.
    flush_to_widget(show_hunters_, show_survivors_, show_props_,
                    show_distance_, show_box_, show_silhouette_,
                    show_tracer_, show_dot_, hunter_proximity_,
                    tracer_origin_, silhouette_alpha_, max_distance_);

    // ESP doesn't need a scene — widget draws from whatever CameraSampler
    // has cached. Report Priming (engaged intent, no visible effect) when
    // the global HUD switch is off, Engaged otherwise.
    (void)t;
    return HudManager::instance().global_enabled() ? Phase::Engaged : Phase::Priming;
}

}  // namespace dxs::procedure
