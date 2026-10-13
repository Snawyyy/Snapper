// CompManager: effects from F's Plugins (CompKind::kFsp), see
// docs/features/fsp-effects.md.

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/comp_fsp.h"
#include "anim/comp_knobs.h"
#include "base/text.h"
#include "edit/comp_manager.h"

namespace snapper {

QString CompManager::EffectName(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) >= 0 &&
         static_cast<int>(effect.kind) < kCompKindCount);
  const bool is_plugin = effect.kind == CompKind::kFsp;
  if (!is_plugin) {
    return KindName(effect.kind);
  }
  const FspEffectInfo* info = FspInfoOf(effect);
  const bool is_known = info != nullptr;
  assert(!is_known || !info->name.isEmpty() || !info->id.isEmpty());
  if (is_known) {
    return info->name.isEmpty() ? info->id : info->name;
  }
  return Tr("%1 (missing)").arg(effect.plugin);
}

QString CompManager::EffectHelp(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) >= 0 &&
         static_cast<int>(effect.kind) < kCompKindCount);
  const bool is_plugin = effect.kind == CompKind::kFsp;
  if (!is_plugin) {
    return KindHelp(effect.kind);
  }
  assert(effect.kept_params.isEmpty() || FspInfoOf(effect) == nullptr);
  const QString why = WhyNoPlugin(effect.plugin);
  const bool is_usable = why.isEmpty();
  return is_usable ? KindHelp(effect.kind)
                   : Tr("%1 It is kept as it was saved and does nothing.")
                         .arg(why);
}

QString CompManager::WhyNoPlugin(const QString& id) {
  const bool has_id = !id.isEmpty();
  if (!has_id) {
    return Tr("This effect names no plugin.");
  }
  const FspEffectInfo* info = FindFspEffect(id);
  const bool is_known = info != nullptr;
  if (!is_known) {
    return Tr("The plugin %1 is not in this Snapper.").arg(id);
  }
  assert(info->id == id);
  assert(info->slot_count >= 0);
  return WhyNoFsp(*info);
}

Result<CompEffectId> CompManager::AddPlugin(ShotId shot, const QString& id) {
  assert(history_ != nullptr);
  const QString why = WhyNoPlugin(id);
  const bool can_add = why.isEmpty();
  if (!can_add) {
    return std::unexpected(Error{why});
  }
  CompEffect effect;
  effect.kind = CompKind::kFsp;
  effect.plugin = id;
  effect.blend = BlendMode::kNormal;
  // No keys: every value is the plugin's own default until changed.
  assert(FspInfoOf(effect) != nullptr);
  const QString label = Tr("Add %1").arg(EffectName(effect));
  const int top = static_cast<int>(Of(shot).size());
  return Insert(shot, std::move(effect), top, label);
}

QColor CompManager::KnobColor(const CompEffect& effect, CompKnob knob,
                              Frame frame) {
  assert(frame.index() >= 0);
  assert(static_cast<int>(knob) < kCompKnobCount);
  const double value = std::clamp(KnobAt(effect, knob, frame), 0.0,
                                  static_cast<double>(0xFFFFFF));
  return QColor::fromRgb(static_cast<QRgb>(std::lround(value)) | 0xFF000000u);
}

Result<void> CompManager::SetKnobColor(ShotId shot, CompEffectId id,
                                       CompKnob knob, QColor color) {
  assert(color.isValid());
  assert(static_cast<int>(knob) < kCompKnobCount);
  const CompEffect* effect = Find(shot, id);
  const bool is_present = effect != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("That effect no longer exists.")});
  }
  const bool is_used = KnobSpecOf(*effect, knob).has_value();
  if (!is_used) {
    return std::unexpected(Error{Tr("That effect has no such control.")});
  }
  const QRgb rgb = color.rgb() & 0xFFFFFFu;
  const QString label = QStringLiteral("%1 %2").arg(
      EffectName(*effect), KnobLabel(*effect, knob).toLower());
  return Edit(
      shot, id, label,
      [knob, rgb](CompEffect* edited) -> Result<void> {
        auto& channel = edited->knobs[static_cast<size_t>(knob)];
        channel.keys = {Key<double>{Frame(0), static_cast<double>(rgb),
                                    Ease::kStep}};
        return {};
      },
      color);
}

}  // namespace snapper
