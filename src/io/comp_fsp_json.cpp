#include "io/comp_fsp_json.h"

#include <QJsonDocument>

#include <cassert>
#include <map>
#include <vector>

#include "fsp/fsp_effects.h"

namespace snapper {
namespace {

// A plugin declares at most this many parameters.
constexpr size_t kMaxParams = 256;

// The plugin effect's values can be read by name: nullptr when its
// plugin is missing or too big for an effect (then the file's values
// are kept as they are).
const FspEffectInfo* UsableInfo(const CompEffect& effect) {
  assert(effect.kind == CompKind::kFsp);
  const bool has_plugin = !effect.plugin.isEmpty();
  if (!has_plugin) {
    return nullptr;
  }
  const FspEffectInfo* info = FindFspEffect(effect.plugin);
  const bool is_usable = info != nullptr && FitsCompSlots(info->slot_count);
  assert(!is_usable || info->params.size() <= kMaxParams);
  return is_usable ? info : nullptr;
}

// One file key per parameter: its name, " (n)" on a repeat, "slot n"
// for a parameter with no name.
std::vector<QString> ParamKeys(const FspEffectInfo& info) {
  assert(info.params.size() <= kMaxParams);
  std::vector<QString> keys;
  std::map<QString, int> seen;
  for (const FspParam& param : info.params) {
    const bool has_name = !param.name.isEmpty();
    const QString base =
        has_name ? param.name : QStringLiteral("slot %1").arg(param.slot);
    const int count = ++seen[base];
    const bool is_repeat = count > 1;
    keys.push_back(is_repeat ? QStringLiteral("%1 (%2)").arg(base).arg(count)
                             : base);
  }
  assert(keys.size() == info.params.size());
  return keys;
}

const Channel<double>& SlotChannel(const CompEffect& effect, int slot) {
  assert(slot >= 0 && slot < kCompParamSlots);
  assert(effect.knobs.size() == static_cast<size_t>(kCompKnobCount));
  return effect.knobs[static_cast<size_t>(ParamKnob(slot))];
}

Channel<double>* SlotChannel(CompEffect* effect, int slot) {
  assert(effect != nullptr);
  assert(slot >= 0 && slot < kCompParamSlots);
  return &effect->knobs[static_cast<size_t>(ParamKnob(slot))];
}

bool IsPoint(const FspParam& param) {
  assert(param.slot >= 0);
  assert(static_cast<int>(param.type) >= 0);
  return param.type == FspParamType::kPoint;
}

// False for a parameter whose slots run past the plugin's (a broken
// setup): it is neither read nor written.
bool HasSlots(const FspParam& param, const FspEffectInfo& info) {
  assert(FitsCompSlots(info.slot_count));
  assert(info.params.size() <= kMaxParams);
  const int width = IsPoint(param) ? 2 : 1;
  return param.slot >= 0 && param.slot + width <= info.slot_count;
}

}  // namespace

QJsonObject FspParamsToJson(const CompEffect& effect) {
  assert(effect.kind == CompKind::kFsp);
  const FspEffectInfo* info = UsableInfo(effect);
  const bool is_kept = info == nullptr;
  if (is_kept) {
    return QJsonDocument::fromJson(effect.kept_params.toUtf8()).object();
  }
  const std::vector<QString> keys = ParamKeys(*info);
  QJsonObject params;
  for (size_t i = 0; i < info->params.size(); ++i) {
    const FspParam& param = info->params[i];
    const bool has_slots = HasSlots(param, *info);
    if (!has_slots) {
      continue;
    }
    const bool is_point = IsPoint(param);
    const auto& first = SlotChannel(effect, param.slot);
    if (!is_point) {
      const bool has_keys = !first.keys.empty();
      if (has_keys) {
        params.insert(keys[i], ChannelToJson(first));
      }
      continue;
    }
    const auto& second = SlotChannel(effect, param.slot + 1);
    const bool has_keys = !first.keys.empty() || !second.keys.empty();
    if (has_keys) {
      params.insert(keys[i], QJsonObject{{"x", ChannelToJson(first)},
                                         {"y", ChannelToJson(second)}});
    }
  }
  assert(params.size() <= static_cast<qsizetype>(info->params.size()));
  return params;
}

void FspParamsFromJson(const QJsonValue& value, CompEffect* effect,
                       JsonIssues* issues) {
  assert(effect != nullptr && effect->kind == CompKind::kFsp);
  assert(issues != nullptr);
  const QJsonObject params = value.toObject();
  const FspEffectInfo* info = UsableInfo(*effect);
  const bool is_kept = info == nullptr;
  if (is_kept) {
    effect->kept_params = QString::fromUtf8(
        QJsonDocument(params).toJson(QJsonDocument::Compact));
    return;
  }
  const std::vector<QString> keys = ParamKeys(*info);
  for (size_t i = 0; i < info->params.size(); ++i) {
    const FspParam& param = info->params[i];
    const bool has_slots = HasSlots(param, *info);
    if (!has_slots) {
      continue;
    }
    const QJsonValue saved = params.value(keys[i]);
    const bool is_point = IsPoint(param);
    if (!is_point) {
      *SlotChannel(effect, param.slot) =
          ChannelFromJson(saved, 0.0, issues);
      continue;
    }
    const QJsonObject point = saved.toObject();
    *SlotChannel(effect, param.slot) =
        ChannelFromJson(point.value("x"), 0.0, issues);
    *SlotChannel(effect, param.slot + 1) =
        ChannelFromJson(point.value("y"), 0.0, issues);
  }
}

}  // namespace snapper
