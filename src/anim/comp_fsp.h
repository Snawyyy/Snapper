#ifndef SNAPPER_ANIM_COMP_FSP_H_
#define SNAPPER_ANIM_COMP_FSP_H_

#include <QString>

#include <optional>

#include "anim/comp_knobs.h"
#include "base/frame.h"
#include "fsp/fsp_effects.h"
#include "model/comp.h"

namespace snapper {

// How a kFsp Comp effect's knobs map onto its plugin's parameters
// (fsp/fsp_effects.h): slot i of the plugin is CompKnob kParam0 + i.

// Where one value slot sits in its plugin's parameters: the parameter
// and, for a point, which half (0 x, 1 y; 0 for the rest).
struct FspSlot final {
  const FspParam* param = nullptr;
  int part = 0;
};

// The plugin effect runs: nullptr when effect is not kFsp, names a
// plugin this Snapper lacks, or the plugin has too many value slots.
const FspEffectInfo* FspInfoOf(const CompEffect& effect);
// Why the plugin cannot be an effect; empty when it can.
QString WhyNoFsp(const FspEffectInfo& info);
// The parameter behind slot of info; nothing past its last slot.
std::optional<FspSlot> FspSlotOf(const FspEffectInfo& info, int slot);
// The spec of a plugin knob (kParam0 on); nothing when effect's plugin
// does not use it.
std::optional<KnobSpec> FspKnobSpec(const CompEffect& effect, CompKnob knob);
// The plugin's own word for a plugin knob ("Width", "Center x").
QString FspKnobName(const CompEffect& effect, CompKnob knob);
// True for numbers that may change across the shot (sliders, floats,
// angles, points); toggles, choices and colors hold one value.
bool IsFspKeyable(FspParamType type);
// True for values that are whole numbers (sliders, toggles, choices,
// colors).
bool IsFspWhole(FspParamType type);
// Every slot's value at frame, sampled and clamped like any knob,
// whole types rounded: what RunFspEffect is handed.
FspValues FspValuesAt(const CompEffect& effect, const FspEffectInfo& info,
                      Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_COMP_FSP_H_
