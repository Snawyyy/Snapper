// CompManager: effects from F's Plugins (CompKind::kFsp), see
// docs/features/fsp-effects.md.

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "anim/comp_fsp.h"
#include "anim/comp_knobs.h"
#include "base/text.h"
#include "edit/comp_manager.h"

namespace snapper {
namespace {

struct PluginWords final {
  const char* id;
  const char* help;
};

// What each plugin does, in a line; its own readme is in Japanese.
constexpr std::array<PluginWords, 19> kPluginHelp = {{
    {"AlphaFill", "Fills the drawn parts with one color, or keys a color "
                  "out."},
    {"BurstLines", "Lines bursting out from a point, for impacts."},
    {"CellLineEraser", "Removes picked line colors and fills the gap with "
                       "the colors around them."},
    {"ColorSwitch", "Swaps, keys out or pulls out up to 32 picked "
                    "colors."},
    {"FloodFill", "A paint bucket: fills a closed area from a point."},
    {"InkingLineBlur", "Blurs the lines and keeps the blur only on picked "
                       "colors, for soft colored lines."},
    {"InnerFlare", "A soft glow inside the shape's edge, or outside it."},
    {"InnerFlareRot", "A glow inside the edge pushed to one side, like "
                      "light from a direction."},
    {"LineExtraction", "Draws lines from the picture's edges in one "
                       "color."},
    {"Outline", "Draws a line around the drawn parts, inside, outside or "
                "both."},
    {"RadialLines", "Focus lines closing in on a point, as in manga."},
    {"RimFill", "Fills a band along the shape's edge with a color or the "
                "color under it."},
    {"SelectiveColorBlur", "Blurs only the picked colors."},
    {"SpeedLines", "Parallel speed lines across the frame, at any angle."},
    {"Sputtering", "Spray dots along edges, like airbrush splatter."},
    {"Star", "A cross star filter: streaks from the bright parts."},
    {"Thin", "Thins picked line colors by a few pixels."},
    {"TouchLines", "Speed lines from the target's edges, straight or "
                   "bursting."},
    {"WhiteKeying", "Makes white (or a picked color) see-through."},
}};

}  // namespace

QString CompManager::PluginHelp(const QString& id) {
  assert(!kPluginHelp.empty());
  assert(kPluginHelp.size() < 512);
  for (const PluginWords& words : kPluginHelp) {
    const bool is_it = id == QLatin1String(words.id);
    if (is_it) {
      return Tr(words.help);
    }
  }
  return KindHelp(CompKind::kFsp);
}

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
  return is_usable ? PluginHelp(effect.plugin)
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

Result<CompEffectId> CompManager::AddPlugin(ShotId shot, const QString& id,
                                            const CompTarget& target,
                                            int index) {
  assert(history_ != nullptr);
  assert(target.layer.IsValid() || target.pieces.empty());
  const QString why = WhyNoPlugin(id);
  const bool can_add = why.isEmpty();
  if (!can_add) {
    return std::unexpected(Error{why});
  }
  CompEffect effect;
  effect.kind = CompKind::kFsp;
  effect.plugin = id;
  effect.target = target;
  effect.blend = BlendMode::kNormal;
  // No keys: every value is the plugin's own default until changed.
  assert(FspInfoOf(effect) != nullptr);
  const QString label = Tr("Add %1").arg(EffectName(effect));
  return Insert(shot, std::move(effect), index, label);
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
