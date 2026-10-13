#ifndef SNAPPER_MODEL_COMP_H_
#define SNAPPER_MODEL_COMP_H_

#include <QColor>
#include <QString>

#include <array>
#include <cassert>
#include <compare>
#include <vector>

#include "base/id.h"
#include "model/key.h"
#include "model/layer.h"

namespace snapper {

struct CompEffectTag;
using CompEffectId = Id<CompEffectTag>;

constexpr int kMaxCompEffectsPerShot = 32;
// Picked target colors and color swap pairs of one effect.
constexpr int kMaxCompColors = 16;
constexpr int kMaxGradientStops = 8;
constexpr int kMaxGradientSteps = 8;

// The finishing effects of the Comp tab, run over a shot's finished
// frame (see docs/features/compositing.md).
enum class CompKind {
  kLineSmoother,      // Smooths stair steps without blurring.
  kColorSwap,         // Swaps picked colors for others.
  kTransmittedLight,  // Glow by distance from a shape's edge.
  kRimLight,          // A band of light on edges facing the light.
  kGradient,          // A ramp laid over the target.
  kMotionBlur,        // Smears along a direction with streaks.
  kSparkle,           // Star streaks and glow from bright parts.
  kSpeedLines,        // Burst or parallel lines.
  kFinish,            // Grain, scanlines or posterize.
  kFsp,               // A plugin from F's Plugins (fsp/fsp_effects.h).
};
inline constexpr int kCompKindCount = 10;

// The one shared set of blend modes, grouped the way artists know it.
// Saved by name, so reordering never changes a finished shot.
enum class BlendMode {
  kNormal,
  kDarken,
  kMultiply,
  kColorBurn,
  kLinearBurn,
  kLighten,
  kScreen,
  kColorDodge,
  kAdd,
  kOverlay,
  kSoftLight,
  kHardLight,
  kDifference,
  kExclusion,
  kSubtract,
  kHue,
  kSaturation,
  kColor,
  kLuminosity,
};
inline constexpr int kBlendModeCount = 19;

// The keyed numbers of an effect. Each kind uses some of them, with its
// own label, range and default (anim/comp_knobs.h). Points are in
// canvas pixels from the canvas middle: on a layer target in the
// layer's own space, so they move with it.
enum class CompKnob {
  kAmount,    // Strength, 0 to 1.
  kSize,      // A width, length or size in canvas pixels.
  kAngle,     // Degrees, 0 up, clockwise.
  kSoftness,  // A 0 to 1 shape knob (falloff, feather, threshold).
  kCount,     // A count (streaks, density, steps, levels).
  kRate,      // Frames between changes; 0 holds.
  kAx,
  kAy,
  kBx,
  kBy,
  // The first of a plugin's value slots (kFsp): slot i is kParam0 + i,
  // labelled, ranged and saved by the plugin's own parameters.
  kParam0,
};
// The knobs every kind may use, before the plugin slots.
inline constexpr int kCompFixedKnobCount = 10;
// A plugin with more value slots than this is refused.
inline constexpr int kCompParamSlots = 32;
inline constexpr int kCompKnobCount = kCompFixedKnobCount + kCompParamSlots;

// The knob of a plugin's value slot.
inline CompKnob ParamKnob(int slot) {
  assert(slot >= 0 && slot < kCompParamSlots);
  assert(static_cast<int>(CompKnob::kParam0) == kCompFixedKnobCount);
  return static_cast<CompKnob>(kCompFixedKnobCount + slot);
}

// The slot of a plugin knob; -1 for the fixed knobs.
inline int ParamSlotOf(CompKnob knob) {
  const int index = static_cast<int>(knob);
  assert(index >= 0 && index < kCompKnobCount);
  assert(static_cast<int>(CompKnob::kParam0) == kCompFixedKnobCount);
  return index >= kCompFixedKnobCount ? index - kCompFixedKnobCount : -1;
}

// True when a plugin with slot_count value slots fits an effect.
inline bool FitsCompSlots(int slot_count) {
  assert(kCompParamSlots > 0);
  assert(kCompKnobCount > kCompFixedKnobCount);
  return slot_count >= 0 && slot_count <= kCompParamSlots;
}

enum class GradientShape {
  kLinear,      // Between A and B.
  kRadial,      // A circle around A reaching B.
  kAngle,       // Sweeps around A from the direction of B.
  kDiamond,     // A diamond around A reaching B.
  kFourCorner,  // A color on each corner of the box A to B.
  kFromEdge,    // By distance from the target's outline, inward.
};
inline constexpr int kGradientShapeCount = 6;

// One color of a ramp; the color's alpha is the stop's opacity.
struct GradientStop final {
  double at = 0.0;
  QColor color = Qt::black;

  bool operator==(const GradientStop&) const = default;
};

struct ColorSwap final {
  QColor from = Qt::black;
  QColor to = Qt::white;

  bool operator==(const ColorSwap&) const = default;
};

// One layer an effect works on: those pieces of its doll, or, with no
// pieces, the whole layer.
struct CompTargetPart final {
  LayerId layer{};
  std::vector<QString> pieces{};

  auto operator<=>(const CompTargetPart&) const = default;
};

// What an effect works on. No parts is everything below (the whole
// frame, in screen space); each part narrows it to a layer, or to some
// pieces of a doll, and the effect works on the union of the parts, so
// pieces of several dolls can share one effect; colors narrow any of
// them to pixels of those colors. They combine: "skin color on the
// dolls Mika and Rei only".
struct CompTarget final {
  std::vector<CompTargetPart> parts{};
  std::vector<QColor> colors{};

  bool operator==(const CompTarget&) const = default;
  // True when the effect is narrowed to layers (not "everything").
  bool HasLayers() const { return !parts.empty(); }
};

// One effect of a shot's comp stack. Colors are stored as colors, not
// palette slots, so a doll edit never changes a finished shot.
struct CompEffect final {
  CompEffectId id{};
  CompKind kind = CompKind::kRimLight;
  // The eye on its row.
  bool is_on = true;
  BlendMode blend = BlendMode::kNormal;
  CompTarget target{};
  QColor color = Qt::white;
  // A kind's variant: transmitted light outside (0) or inside (1);
  // speed lines burst (0) or parallel (1); finish grain (0), scanlines
  // (1) or posterize (2).
  int style = 0;
  // Rim light and gradients aimed by the shot's Light instead of their
  // own angle.
  bool uses_shot_light = false;
  GradientShape shape = GradientShape::kLinear;
  // Bottom of the ramp first; four-corner uses the first four as the
  // corners A, top right, B, bottom left.
  std::vector<GradientStop> stops{};
  std::vector<ColorSwap> swaps{};
  // By CompKnob; a knob without keys is its kind's default.
  std::array<Channel<double>, kCompKnobCount> knobs{};
  // kFsp: the plugin's id (FspEffectInfo::id); its values are the knobs
  // from kParam0 on.
  QString plugin{};
  // kFsp naming a plugin this Snapper lacks: its saved values, kept as
  // the file had them (compact JSON) so a save writes them back
  // unchanged. Empty otherwise.
  QString kept_params{};

  bool operator==(const CompEffect&) const = default;
};

const CompEffect* FindCompEffect(const std::vector<CompEffect>& comp,
                                 CompEffectId id);
CompEffect* FindCompEffect(std::vector<CompEffect>* comp, CompEffectId id);
// One more than the highest id in comp.
CompEffectId NextCompEffectId(const std::vector<CompEffect>& comp);
// Index of id in comp, or -1.
int CompEffectIndex(const std::vector<CompEffect>& comp, CompEffectId id);

}  // namespace snapper

#endif  // SNAPPER_MODEL_COMP_H_
