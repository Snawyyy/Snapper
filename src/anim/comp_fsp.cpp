#include "anim/comp_fsp.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/sampler.h"
#include "base/text.h"
#include "fsp/olm_smoother_info.h"

namespace snapper {
namespace {

// Points and other numbers whose plugin gives no range of its own.
constexpr double kFar = 4000.0;
constexpr double kTurn = 360.0;
// A point is in percent of the picture; these are how far out it goes.
constexpr double kPointLow = -100.0;
constexpr double kPointHigh = 200.0;
constexpr double kWhite = 0xFFFFFF;
// A plugin declares at most this many parameters (more are a broken
// setup, never read).
constexpr size_t kMaxParams = 256;

// The range of one slot, by its parameter's kind. Plugins state ranges
// for numbers; toggles, choices and colors have fixed ones.
KnobSpec SlotSpec(const FspParam& param, double neutral) {
  assert(param.slot >= 0);
  assert(std::isfinite(neutral));
  const bool has_range = param.high > param.low;
  // The fader runs over the plugin's slider range, which is the useful
  // part of the range it accepts (a width of 0 to 10, not 0 to 200).
  const bool has_slider = param.slider_high > param.slider_low;
  const double low = has_slider ? param.slider_low
                     : has_range ? param.low
                                 : -kFar;
  const double high = has_slider ? param.slider_high
                      : has_range ? param.high
                                  : kFar;
  switch (param.type) {
    case FspParamType::kSlider:
    case FspParamType::kFloat:
      return KnobSpec{"", std::min(low, neutral), std::max(high, neutral),
                      neutral, false};
    case FspParamType::kPoint:
      // In percent of the picture, which may lie outside it.
      return KnobSpec{"", std::min(kPointLow, neutral),
                      std::max(kPointHigh, neutral), neutral, false};
    case FspParamType::kAngle: {
      // A plugin's angle may be unbounded; at least one turn is offered.
      const bool is_turn = has_range && high - low >= kTurn;
      return KnobSpec{"", is_turn ? low : std::min(0.0, neutral),
                      is_turn ? high : std::max(kTurn, neutral), neutral,
                      true};
    }
    case FspParamType::kCheckbox:
      return KnobSpec{"", 0.0, 1.0, std::clamp(neutral, 0.0, 1.0), false};
    case FspParamType::kPopup: {
      const double last =
          std::max(1.0, static_cast<double>(param.choices.size()));
      return KnobSpec{"", 1.0, last, std::clamp(neutral, 1.0, last), false};
    }
    case FspParamType::kColor:
      return KnobSpec{"", 0.0, kWhite, std::clamp(neutral, 0.0, kWhite),
                      false};
  }
  return KnobSpec{"", low, high, neutral, false};
}

// The plugin's default for slot (its own setup decides a point's two).
double NeutralOf(const FspEffectInfo& info, int slot) {
  assert(slot >= 0 && slot < info.slot_count);
  const FspValues defaults = DefaultFspValues(info);
  const bool is_known = static_cast<int>(defaults.size()) > slot;
  assert(is_known);
  if (!is_known) {
    return 0.0;
  }
  const double value = defaults[static_cast<size_t>(slot)];
  return std::isfinite(value) ? value : 0.0;
}

}  // namespace

const FspEffectInfo* FspInfoOf(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  const bool is_plugin =
      effect.kind == CompKind::kFsp && !effect.plugin.isEmpty();
  if (!is_plugin) {
    return nullptr;
  }
  const FspEffectInfo* info = FindFspEffect(effect.plugin);
  const bool is_usable = info != nullptr && WhyNoFsp(*info).isEmpty();
  assert(!is_usable || info->id == effect.plugin);
  return is_usable ? info : nullptr;
}

const FspEffectInfo* ParamInfoOf(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  assert(FitsCompSlots(kSmootherSlotCount));
  const bool is_smoother = effect.kind == CompKind::kLineSmoother;
  return is_smoother ? &OlmSmootherInfo() : FspInfoOf(effect);
}

QString WhyNoFsp(const FspEffectInfo& info) {
  assert(!info.id.isEmpty());
  assert(info.params.size() <= kMaxParams);
  const bool fits = FitsCompSlots(info.slot_count);
  return fits ? QString()
              : Tr("%1 has %2 values; an effect holds at most %3.")
                    .arg(info.name)
                    .arg(info.slot_count)
                    .arg(kCompParamSlots);
}

std::optional<FspSlot> FspSlotOf(const FspEffectInfo& info, int slot) {
  assert(slot >= 0);
  assert(info.params.size() <= kMaxParams);
  for (const FspParam& param : info.params) {
    const int width = param.type == FspParamType::kPoint ? 2 : 1;
    const bool is_here = slot >= param.slot && slot < param.slot + width;
    if (is_here) {
      return FspSlot{&param, slot - param.slot};
    }
  }
  return std::nullopt;
}

std::optional<KnobSpec> FspKnobSpec(const CompEffect& effect, CompKnob knob) {
  assert(static_cast<int>(knob) < kCompKnobCount);
  const int slot = ParamSlotOf(knob);
  const FspEffectInfo* info = ParamInfoOf(effect);
  const bool is_slot = slot >= 0 && info != nullptr && slot < info->slot_count;
  if (!is_slot) {
    return std::nullopt;
  }
  const auto found = FspSlotOf(*info, slot);
  const bool is_found = found.has_value();
  if (!is_found) {
    return std::nullopt;
  }
  assert(found->param != nullptr);
  assert(found->part == 0 || found->part == 1);
  return SlotSpec(*found->param, NeutralOf(*info, slot));
}

QString FspKnobName(const CompEffect& effect, CompKnob knob) {
  assert(static_cast<int>(knob) < kCompKnobCount);
  const int slot = ParamSlotOf(knob);
  const FspEffectInfo* info = ParamInfoOf(effect);
  const bool is_slot = slot >= 0 && info != nullptr;
  if (!is_slot) {
    return QString();
  }
  const auto found = FspSlotOf(*info, slot);
  const bool is_found = found.has_value();
  if (!is_found) {
    return QString();
  }
  assert(found->param != nullptr);
  const bool is_point = found->param->type == FspParamType::kPoint;
  if (!is_point) {
    return found->param->name;
  }
  return QStringLiteral("%1 %2").arg(
      found->param->name,
      found->part == 0 ? QStringLiteral("x") : QStringLiteral("y"));
}

bool IsFspKeyable(FspParamType type) {
  assert(static_cast<int>(type) >= 0);
  const bool is_number = type == FspParamType::kSlider ||
                         type == FspParamType::kFloat ||
                         type == FspParamType::kAngle ||
                         type == FspParamType::kPoint;
  assert(is_number || type == FspParamType::kCheckbox ||
         type == FspParamType::kPopup || type == FspParamType::kColor);
  return is_number;
}

bool IsFspWhole(FspParamType type) {
  assert(static_cast<int>(type) >= 0);
  const bool is_whole = type == FspParamType::kSlider ||
                        type == FspParamType::kCheckbox ||
                        type == FspParamType::kPopup ||
                        type == FspParamType::kColor;
  assert(is_whole || IsFspKeyable(type));
  return is_whole;
}

FspValues FspValuesAt(const CompEffect& effect, const FspEffectInfo& info,
                      Frame frame) {
  assert(frame.index() >= 0);
  assert(FitsCompSlots(info.slot_count));
  FspValues values = DefaultFspValues(info);
  values.resize(static_cast<size_t>(info.slot_count), 0.0);
  for (int slot = 0; slot < info.slot_count; ++slot) {
    const auto found = FspSlotOf(info, slot);
    const bool is_found = found.has_value();
    if (!is_found) {
      continue;
    }
    const auto at = static_cast<size_t>(slot);
    const KnobSpec spec = SlotSpec(*found->param, values[at]);
    const auto& channel = effect.knobs[static_cast<size_t>(ParamKnob(slot))];
    const double value =
        std::clamp(Sample(channel, frame, spec.neutral), spec.low, spec.high);
    const bool is_whole = IsFspWhole(found->param->type);
    values[at] = is_whole ? std::round(value) : value;
  }
  return values;
}

}  // namespace snapper
