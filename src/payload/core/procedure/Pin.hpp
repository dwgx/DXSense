#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  Pins — typed, self-registering, Config-bound configuration points.
//
//  A Pin is a single knob on a Procedure. It has:
//    - a type (bool / float / int / Choice)
//    - a tag (Config key suffix: "radius", "tap_hz", ...)
//    - a label (human-readable)
//    - a default value
//    - optional bounds (min/max for numeric)
//
//  What a Pin gives you "for free":
//    - auto-hydrate from Config on construction (Loom has already loaded
//      Config by the time Procedures register)
//    - auto-persist on write
//    - auto-render in the Procedure inspector (the generic fallback; the
//      procedure can still implement draw_inspector() for bespoke UI)
//    - self-registration into the owning Procedure's pin list so the
//      Procedure author never maintains a manual vector
// ═════════════════════════════════════════════════════════════════════════

#include "Procedure.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace dxs::procedure {

// ─── PinBase ────────────────────────────────────────────────────────────
// Everything the Loom + inspector UI need without knowing the Pin's type.

class PinBase {
public:
    virtual ~PinBase() = default;

    std::string_view tag()    const { return tag_; }
    std::string_view label()  const { return label_; }

    // Rendered key: "procedure.<handle>.<tag>". Computed once on construct.
    std::string_view config_key() const { return config_key_; }

    // Dispatched by the generic inspector renderer — each concrete pin
    // draws whichever control fits its type.
    virtual void draw() = 0;

    // Reset-to-default. Called from the "Restore defaults" flow if the
    // Procedure chooses to honour it; default implementation is type-
    // specific and implemented in the templates below.
    virtual void reset_to_default() = 0;

protected:
    PinBase(Procedure* owner,
            std::string_view tag,
            std::string_view label)
        : tag_(tag), label_(label) {
        // config_key_ = "procedure." + owner.handle + "." + tag. handle
        // is readable because Procedure manifest() is a non-virtual const
        // ref; owner is constructed-enough at Pin ctor time to call it.
        config_key_ = "procedure.";
        config_key_.append(owner->manifest().handle);
        config_key_.push_back('.');
        config_key_.append(tag);
        owner->pins_.push_back(this);
    }

    std::string_view tag_;
    std::string_view label_;
    std::string      config_key_;
};

// ─── Pin<bool> ──────────────────────────────────────────────────────────

class PinBool : public PinBase {
public:
    PinBool(Procedure* owner,
            std::string_view tag, std::string_view label,
            bool defv)
        : PinBase(owner, tag, label), default_(defv),
          value_(Config::instance().get_bool(config_key_, defv)) {}

    bool  get() const { return value_; }
    operator bool() const { return value_; }
    void  set(bool v) {
        if (v == value_) return;
        value_ = v;
        Config::instance().set_bool(config_key_, v);
    }
    PinBool& operator=(bool v) { set(v); return *this; }

    void reset_to_default() override { set(default_); }
    void draw() override;   // implementation in Pin.cpp so we don't
                            // drag theme headers into this file

private:
    bool default_;
    bool value_;
};

// ─── Pin<float> ─────────────────────────────────────────────────────────

class PinFloat : public PinBase {
public:
    struct Bounds { float lo, hi; };
    PinFloat(Procedure* owner,
             std::string_view tag, std::string_view label,
             float defv, Bounds b)
        : PinBase(owner, tag, label), default_(defv), bounds_(b),
          value_(clamp(Config::instance().get_float(config_key_, defv), b)) {}

    float get() const { return value_; }
    operator float() const { return value_; }
    void  set(float v) {
        v = clamp(v, bounds_);
        if (v == value_) return;
        value_ = v;
        Config::instance().set_float(config_key_, v);
    }
    PinFloat& operator=(float v) { set(v); return *this; }

    float min() const { return bounds_.lo; }
    float max() const { return bounds_.hi; }
    void  reset_to_default() override { set(default_); }
    void  draw() override;

private:
    static float clamp(float v, Bounds b) {
        return std::clamp(v, b.lo, b.hi);
    }
    float  default_;
    Bounds bounds_;
    float  value_;
};

// ─── Pin<int> ───────────────────────────────────────────────────────────

class PinInt : public PinBase {
public:
    struct Bounds { int lo, hi; };
    PinInt(Procedure* owner,
           std::string_view tag, std::string_view label,
           int defv, Bounds b)
        : PinBase(owner, tag, label), default_(defv), bounds_(b),
          value_(std::clamp(
              Config::instance().get_int(config_key_, defv), b.lo, b.hi)) {}

    int  get() const { return value_; }
    operator int() const { return value_; }
    void set(int v) {
        v = std::clamp(v, bounds_.lo, bounds_.hi);
        if (v == value_) return;
        value_ = v;
        Config::instance().set_int(config_key_, v);
    }
    PinInt& operator=(int v) { set(v); return *this; }

    int  min() const { return bounds_.lo; }
    int  max() const { return bounds_.hi; }
    void reset_to_default() override { set(default_); }
    void draw() override;

private:
    int    default_;
    Bounds bounds_;
    int    value_;
};

// ─── Pin<choice> ────────────────────────────────────────────────────────
// A mutex selection over a fixed label list. Rendered as segmented.

class PinChoice : public PinBase {
public:
    PinChoice(Procedure* owner,
              std::string_view tag, std::string_view label,
              std::initializer_list<const char*> options,
              int defv)
        : PinBase(owner, tag, label), default_(defv) {
        const size_t n = std::min(options.size(), options_.size());
        for (size_t i = 0; i < n; ++i) options_[i] = *(options.begin() + i);
        count_ = static_cast<int>(n);
        value_ = std::clamp(
            Config::instance().get_int(config_key_, defv), 0, count_ - 1);
    }

    int  get() const { return value_; }
    operator int() const { return value_; }
    void set(int v) {
        v = std::clamp(v, 0, count_ - 1);
        if (v == value_) return;
        value_ = v;
        Config::instance().set_int(config_key_, v);
    }
    PinChoice& operator=(int v) { set(v); return *this; }

    int                       count()   const { return count_; }
    std::string_view          option(int i) const { return options_[i]; }
    const char* const*        options_raw() const { return options_.data(); }

    void reset_to_default() override { set(default_); }
    void draw() override;

private:
    // Fixed-capacity to keep PinChoice POD-ish; 8 options is plenty for
    // the kind of choice a procedure needs to expose.
    static constexpr size_t kCap = 8;
    std::array<const char*, kCap> options_{};
    int default_;
    int count_ = 0;
    int value_ = 0;
};

}  // namespace dxs::procedure
