#ifndef SNAPPER_ANIM_COMP_KNOBS_H_
#define SNAPPER_ANIM_COMP_KNOBS_H_

#include <QPointF>

#include <optional>

#include "base/frame.h"
#include "model/comp.h"
#include "model/shot.h"

namespace snapper {

// How one kind uses one knob: its label, range and value with no keys.
// Points (kAx to kBy) are set from stage handles, so they have a spec
// but no fader.
struct KnobSpec final {
  const char* label = "";
  double low = 0.0;
  double high = 1.0;
  double neutral = 0.0;
  // Shown as a dial (degrees) instead of a fader.
  bool is_angle = false;
};

// The spec of knob for kind; nothing when kind does not use it.
std::optional<KnobSpec> KnobSpecOf(CompKind kind, CompKnob knob);
// True for kinds placed by points on the stage (A, and B for most).
bool HasPoints(const CompEffect& effect);
bool HasPointB(const CompEffect& effect);

// The value of knob at frame: its keys through the one sampler, else
// the kind's default, clamped to the spec's range.
double KnobAt(const CompEffect& effect, CompKnob knob, Frame frame);
// Point A or B at frame (canvas pixels from the canvas middle).
QPointF PointAAt(const CompEffect& effect, Frame frame);
QPointF PointBAt(const CompEffect& effect, Frame frame);
// The light angle effect uses at frame: the shot's Light when it picks
// it, else its own angle knob.
double LightAt(const Shot& shot, const CompEffect& effect, Frame frame);
// The shot's Light at frame; 315 (from the upper left) without keys.
double ShotLightAt(const Shot& shot, Frame frame);
inline constexpr double kDefaultShotLight = 315.0;

}  // namespace snapper

#endif  // SNAPPER_ANIM_COMP_KNOBS_H_
