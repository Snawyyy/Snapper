// CompManager: which controls the Comp panel shows for an effect, and
// their words.

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/comp_fsp.h"
#include "anim/comp_knobs.h"
#include "base/text.h"
#include "edit/comp_manager.h"

namespace snapper {
namespace {

// A plugin declares at most this many parameters.
constexpr size_t kMaxParams = 256;

// Digits after the point for a plugin float, by how far it reaches.
int FloatDecimals(const KnobSpec& spec) {
  assert(spec.low <= spec.high);
  assert(std::isfinite(spec.high - spec.low));
  const double span = spec.high - spec.low;
  return span <= 2.0 ? 3 : span <= 200.0 ? 2 : 1;
}

// The panel row of one plugin knob.
CompRow PluginRow(const CompEffect& effect, const FspParam& param,
                  CompKnob knob) {
  assert(effect.kind == CompKind::kFsp);
  assert(ParamSlotOf(knob) >= 0);
  CompRow row;
  row.knob = knob;
  row.label = FspKnobName(effect, knob);
  row.is_keyable = IsFspKeyable(param.type);
  const auto spec = KnobSpecOf(effect, knob);
  const int decimals = spec.has_value() ? FloatDecimals(*spec) : 2;
  switch (param.type) {
    case FspParamType::kSlider:
      break;
    case FspParamType::kFloat:
      row.decimals = decimals;
      row.step = std::pow(10.0, 1 - decimals);
      break;
    case FspParamType::kAngle:
      row.kind = CompRowKind::kDial;
      row.decimals = 1;
      break;
    case FspParamType::kPoint:
      row.decimals = 1;
      break;
    case FspParamType::kCheckbox:
      row.kind = CompRowKind::kToggle;
      break;
    case FspParamType::kPopup:
      row.kind = CompRowKind::kChoice;
      row.choices = param.choices;
      break;
    case FspParamType::kColor:
      row.kind = CompRowKind::kColor;
      break;
  }
  return row;
}

// A built-in kind's row for knob: a dial for angles, else a fader.
CompRow KindRow(const CompEffect& effect, CompKnob knob) {
  const auto spec = KnobSpecOf(effect, knob);
  assert(spec.has_value());
  assert(ParamSlotOf(knob) < 0 || effect.kind == CompKind::kFsp);
  CompRow row;
  row.knob = knob;
  row.kind = spec->is_angle ? CompRowKind::kDial : CompRowKind::kFader;
  row.label = CompManager::KnobLabel(effect, knob);
  const bool is_fraction = spec->high <= 1.0;
  row.decimals = is_fraction ? 2 : 0;
  row.step = is_fraction ? 0.05 : 1.0;
  return row;
}

}  // namespace

QString CompManager::KnobLabel(const CompEffect& effect, CompKnob knob) {
  assert(static_cast<int>(knob) >= 0 &&
         static_cast<int>(knob) < kCompKnobCount);
  const auto spec = KnobSpecOf(effect, knob);
  const bool is_used = spec.has_value();
  if (!is_used) {
    return QString();
  }
  const bool is_plugin_slot = ParamSlotOf(knob) >= 0;
  assert(!is_plugin_slot || effect.kind == CompKind::kFsp);
  return is_plugin_slot ? FspKnobName(effect, knob) : Tr(spec->label);
}

bool CompManager::IsKnobShown(const CompEffect& effect, CompKnob knob) {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const auto spec = KnobSpecOf(effect, knob);
  const bool is_point = knob == CompKnob::kAx || knob == CompKnob::kAy ||
                        knob == CompKnob::kBx || knob == CompKnob::kBy;
  const bool is_unused = !spec.has_value() || is_point;
  if (is_unused) {
    return false;
  }
  const bool is_plugin = effect.kind == CompKind::kFsp;
  if (is_plugin) {
    return IsPluginKnobShown(effect, knob);
  }
  const bool is_lit_angle = knob == CompKnob::kAngle &&
                            effect.uses_shot_light && CanUseShotLight(effect);
  if (is_lit_angle) {
    return false;
  }
  switch (effect.kind) {
    case CompKind::kGradient: {
      const bool is_edge = effect.shape == GradientShape::kFromEdge;
      const bool is_corners = effect.shape == GradientShape::kFourCorner;
      return knob == CompKnob::kSize   ? is_edge
             : knob == CompKnob::kCount ? !is_corners
                                        : true;
    }
    case CompKind::kSpeedLines: {
      const bool is_burst = effect.style == 0;
      return knob == CompKnob::kAngle      ? !is_burst
             : knob == CompKnob::kSoftness ? is_burst
                                           : true;
    }
    case CompKind::kFinish: {
      const bool is_poster = effect.style == 2;
      const bool is_grain = effect.style == 0;
      return knob == CompKnob::kCount  ? is_poster
             : knob == CompKnob::kSize ? !is_poster
             : knob == CompKnob::kRate ? is_grain
                                       : true;
    }
    default:
      return true;
  }
}

bool CompManager::IsPluginKnobShown(const CompEffect& effect,
                                    CompKnob knob) {
  assert(effect.kind == CompKind::kFsp);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const bool is_amount = knob == CompKnob::kAmount;
  const FspEffectInfo* info = FspInfoOf(effect);
  const int slot = ParamSlotOf(knob);
  const bool is_slot = info != nullptr && slot >= 0;
  if (!is_slot) {
    return is_amount && info != nullptr;
  }
  const auto found = FspSlotOf(*info, slot);
  return found.has_value() && !found->param->is_hidden &&
         IsFspKeyable(found->param->type);
}

bool CompManager::IsKeyable(const CompEffect& effect, CompKnob knob) {
  assert(static_cast<int>(knob) < kCompKnobCount);
  const int slot = ParamSlotOf(knob);
  const FspEffectInfo* info = FspInfoOf(effect);
  const bool is_plugin_slot = slot >= 0 && info != nullptr;
  if (!is_plugin_slot) {
    return KnobSpecOf(effect, knob).has_value();
  }
  const auto found = FspSlotOf(*info, slot);
  assert(!found.has_value() || found->param != nullptr);
  return found.has_value() && IsFspKeyable(found->param->type);
}

std::vector<CompRow> CompManager::RowsOf(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  std::vector<CompRow> rows;
  const bool is_plugin = effect.kind == CompKind::kFsp;
  if (!is_plugin) {
    for (int i = 0; i < kCompFixedKnobCount; ++i) {
      const auto knob = static_cast<CompKnob>(i);
      const bool is_shown = IsKnobShown(effect, knob);
      if (is_shown) {
        rows.push_back(KindRow(effect, knob));
      }
    }
    return rows;
  }
  const FspEffectInfo* info = FspInfoOf(effect);
  const bool is_missing = info == nullptr;
  if (is_missing) {
    return rows;
  }
  assert(info->params.size() <= kMaxParams);
  rows.push_back(KindRow(effect, CompKnob::kAmount));
  for (const FspParam& param : info->params) {
    const int width = param.type == FspParamType::kPoint ? 2 : 1;
    const bool is_shown = !param.is_hidden && param.slot >= 0 &&
                          param.slot + width <= info->slot_count;
    for (int part = 0; is_shown && part < width; ++part) {
      rows.push_back(PluginRow(effect, param, ParamKnob(param.slot + part)));
    }
  }
  return rows;
}

}  // namespace snapper
