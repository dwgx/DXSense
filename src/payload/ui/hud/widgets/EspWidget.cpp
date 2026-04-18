#include "EspWidget.hpp"

#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

namespace dxs {

EspWidget::EspWidget() {
    // Pull persisted settings from Config so user tweaks survive eject /
    // reinject. Defaults (declared in the header) take over when a key
    // isn't present yet.
    auto& cfg = Config::instance();
    show_hunters_     = cfg.get_bool ("hud.esp.hunters",       show_hunters_);
    show_survivors_   = cfg.get_bool ("hud.esp.survivors",     show_survivors_);
    show_props_       = cfg.get_bool ("hud.esp.props",         show_props_);
    show_distance_    = cfg.get_bool ("hud.esp.distance",      show_distance_);
    show_box_         = cfg.get_bool ("hud.esp.box",           show_box_);
    show_silhouette_  = cfg.get_bool ("hud.esp.silhouette",    show_silhouette_);
    show_tracer_      = cfg.get_bool ("hud.esp.tracer",        show_tracer_);
    show_dot_         = cfg.get_bool ("hud.esp.dot",           show_dot_);
    hunter_proximity_ = cfg.get_bool ("hud.esp.hunter_proxim", hunter_proximity_);
    tracer_origin_    = cfg.get_int  ("hud.esp.tracer_origin", tracer_origin_);
    silhouette_alpha_ = cfg.get_float("hud.esp.sil_alpha",     silhouette_alpha_);
    max_distance_     = cfg.get_float("hud.esp.max_dist",      max_distance_);
}

namespace {

struct KindStyle {
    const char* label;
    ImVec4      color;
    float       priority;    // higher = drawn on top
    float       radius;
};

KindStyle style_for(int kind) {
    // Full kind table (from the event-log inventory across several matches)
    // so the on-screen label is never "?". Labels look up via L() so the UI
    // language flips automatically.
    switch (kind) {
        case 1:    return { L("esp.kind_hunter"   ).data(), theme::bad,       3.0f, 4.0f };
        case 2:    return { L("esp.kind_survivor" ).data(), theme::good,      2.0f, 3.5f };
        case 3:    return { L("esp.kind_generator").data(), theme::info,      1.0f, 2.5f };
        case 4:    return { L("esp.kind_hook"     ).data(), theme::warn,      1.0f, 2.5f };
        case 5:    return { L("esp.kind_box"      ).data(), theme::accent,    0.5f, 2.5f };
        case 6:    return { L("esp.kind_door"     ).data(), theme::accent,    0.7f, 2.5f };
        case 7:    return { L("esp.kind_pallet"   ).data(), theme::text_muted,0.5f, 2.5f };
        case 8:    return { L("esp.kind_panel"    ).data(), theme::text_muted,0.5f, 2.5f };
        case 9:    return { L("esp.kind_cupboard" ).data(), theme::text_muted,0.4f, 2.5f };
        case 10:   return { L("esp.kind_basement" ).data(), theme::warn,      0.6f, 2.5f };
        case 11:   return { L("esp.kind_crow"     ).data(), theme::text_muted,0.3f, 2.0f };
        case 12:   return { L("esp.kind_switch"   ).data(), theme::info,      0.4f, 2.5f };
        case 18:   return { L("esp.kind_hatch"    ).data(), theme::good,      0.9f, 3.0f };
        case 100:  return { L("esp.kind_lobby"    ).data(), theme::text_muted,0.2f, 2.0f };
        case 222:  return { L("esp.kind_item"     ).data(), theme::text_muted,0.3f, 2.0f };
        case 510:  return { L("esp.kind_spirit"   ).data(), theme::info,      1.5f, 3.0f };
        case 1009: return { L("esp.kind_npc"      ).data(), theme::text_muted,0.2f, 2.0f };
        case 1911: return { L("esp.kind_marker"   ).data(), theme::text_muted,0.2f, 2.0f };
        default:   return { L("esp.kind_unknown"  ).data(), theme::text_muted,0.0f, 2.0f };
    }
}

bool is_hunter(int k)   { return k == 1; }
bool is_survivor(int k) { return k == 2 || k == 510; }
bool is_prop(int k) {
    return k == 3 || k == 4 || k == 5 || k == 6 || k == 7 ||
           k == 8 || k == 9 || k == 10 || k == 11 || k == 12;
}

// Silhouette colours — match the in-match spectator look the user asked for:
// teammates render as a clean warm-white fill, hunters glow red. These live
// at 1.0 alpha so callers can scale freely for distance / proximity fades.
constexpr ImVec4 sil_white     = { 0.95f, 0.97f, 1.00f, 1.0f };   // survivor
constexpr ImVec4 sil_red       = { 1.00f, 0.30f, 0.34f, 1.0f };   // hunter (far)
constexpr ImVec4 sil_red_hot   = { 1.00f, 0.18f, 0.22f, 1.0f };   // hunter (near)
constexpr ImVec4 sil_prop      = { 0.55f, 0.80f, 1.00f, 1.0f };   // prop / gen / hook

// Threshold for "hunter is near" — below this the silhouette uses the hotter
// red and caps alpha at 1.0 so it clearly reads through terrain.
constexpr float k_hunter_near_m = 22.0f;

}  // namespace

void EspWidget::draw(ImDrawList* dl, ImVec2 /*pos*/, ImVec2 size) {
    auto  snap = CameraSampler::instance().snapshot();
    if (!snap.camera_ready) return;

    auto positioned = [](const Vec3& p) {
        return std::fabs(p.x) + std::fabs(p.y) + std::fabs(p.z) > 0.01f;
    };

    // Dead-reckon onto the current render time so markers don't visibly lag
    // the character between sampler ticks (15 Hz sample vs 60 Hz render).
    const double render_now = CameraSampler::now();

    struct Mark {
        std::uint64_t uid;
        int           kind;
        Vec3          pos;
        ImVec2        screen;
        float         dist;
        KindStyle     st;
    };
    std::vector<Mark> marks;
    marks.reserve(snap.units.size());
    const Vec3 eye = snap.cam_pos;
    for (const auto& u : snap.units) {
        if (!positioned(u.pos)) continue;
        if (is_hunter(u.kind)   && !show_hunters_)   continue;
        if (is_survivor(u.kind) && !show_survivors_) continue;
        if (is_prop(u.kind)     && !show_props_)     continue;
        if (snap.player_ready && u.uid == snap.player_uid) continue;

        const Vec3 pred = snap.predict_position(u, render_now);
        const float dx = pred.x - eye.x, dy = pred.y - eye.y, dz = pred.z - eye.z;
        // Raw distance is in NeoX3 world units (decimetres); scale to
        // metres so `max_distance_` (UI slider is in m) and the on-screen
        // "%.0fm" label are truthful.
        const float d = std::sqrt(dx*dx + dy*dy + dz*dz) * theme::world_to_meter;
        if (d > max_distance_) continue;

        const auto sp = snap.project(pred, size.x, size.y);
        if (!sp) continue;
        marks.push_back(Mark{
            u.uid, u.kind, pred,
            ImVec2{ sp->first, sp->second }, d, style_for(u.kind) });
    }
    std::sort(marks.begin(), marks.end(),
              [](const Mark& a, const Mark& b) { return a.st.priority < b.st.priority; });

    const ImU32 outline       = theme::to_u32({ 0.0f, 0.0f, 0.0f, 0.80f });
    const ImU32 outline_heavy = theme::to_u32({ 0.0f, 0.0f, 0.0f, 0.92f });
    // Tracer anchor depends on the user-selected origin. Computed once
    // per frame so every line shares the same root for that style's
    // characteristic "fan" look.
    ImVec2 tracer_root;
    switch (tracer_origin_) {
        case TracerTop:       tracer_root = { size.x * 0.5f, 4.0f };            break;
        case TracerCentre:    tracer_root = { size.x * 0.5f, size.y * 0.5f };   break;
        case TracerCrosshair: tracer_root = { size.x * 0.5f, size.y * 0.5f };   break;
        case TracerBottom:
        default:              tracer_root = { size.x * 0.5f, size.y - 4.0f };   break;
    }

    for (const auto& m : marks) {
        const float sx = m.screen.x;
        const float sy = m.screen.y;
        if (sx < 0 || sy < 0 || sx > size.x || sy > size.y) continue;

        const ImVec2 c(sx, sy);

        // Distance-aware fade for all chrome. Near targets read almost
        // opaque, far ones dim but NEVER below 70 % — the whole point of
        // ESP is to see distant targets, so an aggressive distance fade
        // is self-defeating. The geometric silhouette shrinks with range
        // on its own; alpha stays near-constant so what IS drawn reads.
        float alpha = 1.0f - std::min(1.0f, m.dist / max_distance_);
        alpha = 0.70f + 0.30f * alpha;
        ImVec4 fcol = m.st.color; fcol.w *= alpha;
        const ImU32 fade_col = theme::to_u32(fcol);

        // Common screen-space body box — size approximated from vertical
        // FOV so a 1.8 m person stays ~consistent regardless of range.
        // Long-range floor at 48 px: the geometric projection of a human
        // at 80-180 m is only ~10-24 px and gets swallowed by foliage /
        // wall textures. A 48 px minimum keeps the silhouette readable
        // without inflating near bodies (geometric > 48 at <38 m anyway).
        const float fov_rad  = snap.fov_y * 0.0174533f;
        const float focal_px = size.y / (2.0f * std::tan(std::max(fov_rad, 0.1f) * 0.5f));
        const float body_h   = std::max(48.0f, (focal_px * 1.8f) / std::max(1.0f, m.dist));
        const float body_w   = body_h * 0.44f;
        const ImVec2 body_bl(c.x - body_w * 0.5f, c.y);
        const ImVec2 body_tr(c.x + body_w * 0.5f, c.y - body_h);

        // --- Silhouette (spectator-style through-wall body fill) -----------
        // Solid rounded body shape in team / hunter colour. Draws BEHIND the
        // outline box so the outline frames it cleanly.
        if (show_silhouette_) {
            ImVec4 sil;
            bool   have_sil = true;
            if (is_hunter(m.kind)) {
                const bool near = m.dist < k_hunter_near_m;
                sil = (near && hunter_proximity_) ? sil_red_hot : sil_red;
                if (near && hunter_proximity_) {
                    // Pulse scales alpha for extra "get out" legibility within
                    // close range. Pulse frequency stays slow (~1 Hz) so it
                    // doesn't strobe in peripheral vision.
                    const float t = (float)std::fmod(ImGui::GetTime(), 1.0);
                    const float pulse = 0.85f + 0.15f * std::sin(t * 6.2832f);
                    sil.w *= pulse;
                }
            } else if (is_survivor(m.kind)) {
                sil = sil_white;
            } else if (is_prop(m.kind)) {
                sil = sil_prop;
                sil.w *= 0.55f;   // props are less critical — dim them
            } else {
                have_sil = false;
            }
            if (have_sil) {
                // Silhouette alpha no longer fades hard with distance —
                // the mix is dominated by the user-controlled slider, with
                // only a small distance taper.
                sil.w *= silhouette_alpha_ * (0.80f + 0.20f * alpha);
                dl->AddRectFilled(body_tr, body_bl,
                                  theme::to_u32(sil), body_w * 0.45f);
                // Crisp rim outline at (near) full alpha so the body
                // shape has a defined edge even when fill is translucent.
                ImVec4 rim = sil; rim.w = std::min(1.0f, sil.w * 2.6f);
                dl->AddRect(body_tr, body_bl,
                            theme::to_u32(rim), body_w * 0.45f, 0, 1.4f);
            }
        }

        // --- Tracer --------------------------------------------------------
        if (show_tracer_) {
            dl->AddLine(tracer_root, c, outline, 2.5f);
            dl->AddLine(tracer_root, c, fade_col, 1.2f);
        }

        // --- Outline box ---------------------------------------------------
        if (show_box_) {
            dl->AddRect(body_bl - ImVec2(1, 1), body_tr + ImVec2(1, 1),
                        outline_heavy, theme::radius_sm, 0, 1.5f);
            dl->AddRect(body_bl, body_tr, fade_col,
                        theme::radius_sm, 0, 1.2f);
        }

        // --- Centre dot ---------------------------------------------------
        // Tiny marker — no distance inflation, no halo. The box / tracer
        // carry target identification; the dot is an opt-in breadcrumb.
        if (show_dot_) {
            dl->AddCircleFilled(c, m.st.radius + 0.8f, outline,  14);
            dl->AddCircleFilled(c, m.st.radius,        fade_col, 14);
        }

        // --- Label --------------------------------------------------------

        char lbl[96];
        if (show_distance_) {
            std::snprintf(lbl, sizeof(lbl), "%s  %.0fm", m.st.label, m.dist);
        } else {
            std::snprintf(lbl, sizeof(lbl), "%s", m.st.label);
        }
        const ImVec2 lsz = ImGui::CalcTextSize(lbl);
        const float  base_y = show_box_ ? (c.y + 3.0f)
                                        : (c.y + m.st.radius + 2.0f);
        const ImVec2 lpos(c.x - lsz.x * 0.5f, base_y);
        dl->AddText(ImVec2(lpos.x + 1, lpos.y + 1), outline, lbl);
        dl->AddText(lpos, fade_col, lbl);
    }
}

void EspWidget::draw_settings() {
    bool dirty = false;
    ImGui::TextDisabled("%s", L("esp.targets").data());
    dirty |= theme::checkbox(L("esp.hunters"  ).data(), &show_hunters_);   ImGui::SameLine();
    dirty |= theme::checkbox(L("esp.survivors").data(), &show_survivors_); ImGui::SameLine();
    dirty |= theme::checkbox(L("esp.props"    ).data(), &show_props_);

    ImGui::TextDisabled("%s", L("esp.overlay").data());
    dirty |= theme::checkbox(L("esp.box"       ).data(), &show_box_);        ImGui::SameLine();
    dirty |= theme::checkbox(L("esp.tracer"    ).data(), &show_tracer_);     ImGui::SameLine();
    dirty |= theme::checkbox(L("esp.distance"  ).data(), &show_distance_);
    dirty |= theme::checkbox(L("esp.silhouette").data(), &show_silhouette_); ImGui::SameLine();
    dirty |= theme::checkbox(L("esp.dot"       ).data(), &show_dot_);        ImGui::SameLine();
    dirty |= theme::checkbox(L("esp.hunter_proximity").data(), &hunter_proximity_);

    ImGui::TextDisabled("%s", L("esp.tuning").data());

    // Tracer origin selector — 4 radio buttons in one row. Drives where
    // each tracer line starts from so the user can pick the feel:
    // "radar antenna" (bottom), "dropping signal" (top), centred, or
    // crosshair-rooted.
    ImGui::TextUnformatted(L("esp.tracer_origin").data());
    ImGui::SameLine(0, theme::space_md);
    struct Opt { int v; const char* key; };
    const Opt tracer_opts[] = {
        { TracerBottom,    "esp.tracer_bottom"    },
        { TracerTop,       "esp.tracer_top"       },
        { TracerCentre,    "esp.tracer_centre"    },
        { TracerCrosshair, "esp.tracer_crosshair" },
    };
    for (const auto& opt : tracer_opts) {
        dirty |= ImGui::RadioButton(L(opt.key).data(),
                                    &tracer_origin_, opt.v);
        ImGui::SameLine(0, theme::space_sm);
    }
    ImGui::NewLine();

    dirty |= ImGui::SliderFloat((std::string(L("esp.silhouette_alpha")) + "##esp").c_str(),
                                &silhouette_alpha_, 0.10f, 1.00f, "%.2f");
    dirty |= ImGui::SliderFloat((std::string(L("esp.max_dist")) + "##esp").c_str(),
                                &max_distance_, 10.0f, 200.0f, "%.0f");

    if (dirty) {
        auto& cfg = Config::instance();
        cfg.set_bool ("hud.esp.hunters",       show_hunters_);
        cfg.set_bool ("hud.esp.survivors",     show_survivors_);
        cfg.set_bool ("hud.esp.props",         show_props_);
        cfg.set_bool ("hud.esp.distance",      show_distance_);
        cfg.set_bool ("hud.esp.box",           show_box_);
        cfg.set_bool ("hud.esp.silhouette",    show_silhouette_);
        cfg.set_bool ("hud.esp.tracer",        show_tracer_);
        cfg.set_bool ("hud.esp.dot",           show_dot_);
        cfg.set_bool ("hud.esp.hunter_proxim", hunter_proximity_);
        cfg.set_int  ("hud.esp.tracer_origin", tracer_origin_);
        cfg.set_float("hud.esp.sil_alpha",     silhouette_alpha_);
        cfg.set_float("hud.esp.max_dist",      max_distance_);
    }
}

}  // namespace dxs
