#include "anim/comp_knobs.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/comp_fsp.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

constexpr double kFar = 4000.0;

KnobSpec Strength() { return KnobSpec{"Strength", 0.0, 1.0, 1.0, false}; }

KnobSpec Point(const char* label, double neutral) {
  assert(label != nullptr);
  assert(neutral >= -kFar && neutral <= kFar);
  return KnobSpec{label, -kFar, kFar, neutral, false};
}

// Strength, then the OLM Smoother's own values in the slots.
std::optional<KnobSpec> SmootherKnob(const CompEffect& effect,
                                     CompKnob knob) {
  assert(effect.kind == CompKind::kLineSmoother);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const bool is_amount = knob == CompKnob::kAmount;
  return is_amount ? std::optional(Strength()) : FspKnobSpec(effect, knob);
}

std::optional<KnobSpec> SwapKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kSize:
      return KnobSpec{"Tolerance", 0.0, 120.0, 16.0, false};
    case CompKnob::kSoftness:
      return KnobSpec{"Feather", 0.0, 1.0, 0.5, false};
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> GlowKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kSize:
      return KnobSpec{"Size", 1.0, 400.0, 40.0, false};
    case CompKnob::kSoftness:
      return KnobSpec{"Falloff", 0.0, 1.0, 0.5, false};
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> RimKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kAngle:
      return KnobSpec{"Light", 0.0, 360.0, kDefaultShotLight, true};
    case CompKnob::kSize:
      return KnobSpec{"Width", 1.0, 100.0, 8.0, false};
    case CompKnob::kSoftness:
      return KnobSpec{"Falloff", 0.0, 1.0, 0.5, false};
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> GradientKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kSize:
      return KnobSpec{"Edge size", 1.0, 400.0, 60.0, false};
    case CompKnob::kCount:
      return KnobSpec{"Steps", 0.0, kMaxGradientSteps, 0.0, false};
    case CompKnob::kAx:
      return Point("A x", 0.0);
    case CompKnob::kAy:
      return Point("A y", -300.0);
    case CompKnob::kBx:
      return Point("B x", 0.0);
    case CompKnob::kBy:
      return Point("B y", 300.0);
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> BlurKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kAngle:
      return KnobSpec{"Direction", 0.0, 360.0, 90.0, true};
    case CompKnob::kSize:
      return KnobSpec{"Length", 0.0, 400.0, 40.0, false};
    case CompKnob::kSoftness:
      return KnobSpec{"Streaks", 0.0, 1.0, 0.3, false};
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> SparkleKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kSoftness:
      return KnobSpec{"Threshold", 0.0, 1.0, 0.9, false};
    case CompKnob::kCount:
      return KnobSpec{"Streaks", 2.0, 8.0, 4.0, false};
    case CompKnob::kSize:
      return KnobSpec{"Length", 4.0, 400.0, 60.0, false};
    case CompKnob::kAngle:
      return KnobSpec{"Rotation", 0.0, 360.0, 0.0, true};
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> LinesKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kCount:
      return KnobSpec{"Density", 8.0, 400.0, 80.0, false};
    case CompKnob::kSize:
      return KnobSpec{"Thickness", 1.0, 40.0, 6.0, false};
    case CompKnob::kSoftness:
      return KnobSpec{"Clear middle", 0.0, 1.0, 0.4, false};
    case CompKnob::kAngle:
      return KnobSpec{"Direction", 0.0, 360.0, 90.0, true};
    case CompKnob::kRate:
      return KnobSpec{"Change every", 0.0, 24.0, 2.0, false};
    case CompKnob::kAx:
      return Point("Center x", 0.0);
    case CompKnob::kAy:
      return Point("Center y", 0.0);
    default:
      return std::nullopt;
  }
}

std::optional<KnobSpec> FinishKnob(CompKnob knob) {
  switch (knob) {
    case CompKnob::kAmount:
      return Strength();
    case CompKnob::kSize:
      return KnobSpec{"Size", 1.0, 8.0, 2.0, false};
    case CompKnob::kCount:
      return KnobSpec{"Levels", 2.0, 32.0, 6.0, false};
    case CompKnob::kRate:
      return KnobSpec{"Change every", 0.0, 24.0, 1.0, false};
    default:
      return std::nullopt;
  }
}

// A plugin's own values, plus the Strength every effect mixes by.
std::optional<KnobSpec> PluginKnob(const CompEffect& effect, CompKnob knob) {
  assert(effect.kind == CompKind::kFsp);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const bool is_amount = knob == CompKnob::kAmount;
  return is_amount ? std::optional(Strength()) : FspKnobSpec(effect, knob);
}

}  // namespace

std::optional<KnobSpec> KnobSpecOf(const CompEffect& effect, CompKnob knob) {
  const CompKind kind = effect.kind;
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kCompKindCount);
  assert(static_cast<int>(knob) >= 0 &&
         static_cast<int>(knob) < kCompKnobCount);
  switch (kind) {
    case CompKind::kLineSmoother:
      return SmootherKnob(effect, knob);
    case CompKind::kColorSwap:
      return SwapKnob(knob);
    case CompKind::kTransmittedLight:
      return GlowKnob(knob);
    case CompKind::kRimLight:
      return RimKnob(knob);
    case CompKind::kGradient:
      return GradientKnob(knob);
    case CompKind::kMotionBlur:
      return BlurKnob(knob);
    case CompKind::kSparkle:
      return SparkleKnob(knob);
    case CompKind::kSpeedLines:
      return LinesKnob(knob);
    case CompKind::kFinish:
      return FinishKnob(knob);
    case CompKind::kFsp:
      return PluginKnob(effect, knob);
  }
  return std::nullopt;
}

bool HasPoints(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  const bool is_gradient = effect.kind == CompKind::kGradient &&
                           effect.shape != GradientShape::kFromEdge;
  const bool is_burst =
      effect.kind == CompKind::kSpeedLines && effect.style == 0;
  return is_gradient || is_burst;
}

bool HasPointB(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  return HasPoints(effect) && effect.kind == CompKind::kGradient;
}

double KnobAt(const CompEffect& effect, CompKnob knob, Frame frame) {
  assert(frame.index() >= 0);
  const auto spec = KnobSpecOf(effect, knob);
  const bool is_used = spec.has_value();
  if (!is_used) {
    return 0.0;
  }
  const auto& channel = effect.knobs[static_cast<size_t>(knob)];
  const double value = Sample(channel, frame, spec->neutral);
  assert(spec->low <= spec->high);
  return std::clamp(value, spec->low, spec->high);
}

QPointF PointAAt(const CompEffect& effect, Frame frame) {
  assert(frame.index() >= 0);
  return QPointF(KnobAt(effect, CompKnob::kAx, frame),
                 KnobAt(effect, CompKnob::kAy, frame));
}

QPointF PointBAt(const CompEffect& effect, Frame frame) {
  assert(frame.index() >= 0);
  return QPointF(KnobAt(effect, CompKnob::kBx, frame),
                 KnobAt(effect, CompKnob::kBy, frame));
}

double ShotLightAt(const Shot& shot, Frame frame) {
  assert(frame.index() >= 0);
  const double angle = Sample(shot.light, frame, kDefaultShotLight);
  const double turned = std::fmod(angle, 360.0);
  return turned < 0.0 ? turned + 360.0 : turned;
}

double LightAt(const Shot& shot, const CompEffect& effect, Frame frame) {
  assert(frame.index() >= 0);
  const bool is_shared = effect.uses_shot_light;
  return is_shared ? ShotLightAt(shot, frame)
                   : KnobAt(effect, CompKnob::kAngle, frame);
}

}  // namespace snapper
