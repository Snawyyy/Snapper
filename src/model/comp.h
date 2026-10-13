#ifndef SNAPPER_MODEL_COMP_H_
#define SNAPPER_MODEL_COMP_H_

#include <QColor>
#include <QString>

#include <array>
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
};
inline constexpr int kCompKindCount = 9;

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
};
inline constexpr int kCompKnobCount = 10;

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

// What an effect works on. No layer is everything below (the whole
// frame, in screen space); a layer narrows it to that layer, pieces to
// those pieces of its doll; colors narrow any of them to pixels of
// those colors. They combine: "skin color on the doll Mika only".
struct CompTarget final {
  LayerId layer{};
  std::vector<QString> pieces{};
  std::vector<QColor> colors{};

  bool operator==(const CompTarget&) const = default;
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
