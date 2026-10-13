#include "io/comp_json.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

namespace snapper {
namespace {

// By enum value; the order must match the enums in model/comp.h.
constexpr std::array<const char*, kCompKindCount> kKindNames = {
    "line_smoother", "color_swap", "transmitted_light",
    "rim_light",     "gradient",   "motion_blur",
    "sparkle",       "speed_lines", "finish"};
constexpr std::array<const char*, kBlendModeCount> kBlendNames = {
    "normal",      "darken",     "multiply",   "color_burn", "linear_burn",
    "lighten",     "screen",     "color_dodge", "add",       "overlay",
    "soft_light",  "hard_light", "difference", "exclusion",  "subtract",
    "hue",         "saturation", "color",      "luminosity"};
constexpr std::array<const char*, kGradientShapeCount> kShapeNames = {
    "linear", "radial", "angle", "diamond", "four_corner", "from_edge"};
constexpr std::array<const char*, kCompKnobCount> kKnobNames = {
    "amount", "size", "angle", "softness", "count",
    "rate",   "ax",   "ay",    "bx",       "by"};

QJsonObject TargetToJson(const CompTarget& target) {
  QJsonObject object;
  const bool has_layer = target.layer.IsValid();
  if (has_layer) {
    object.insert("layer", target.layer.value());
  }
  const bool has_pieces = !target.pieces.empty();
  if (has_pieces) {
    QJsonArray pieces;
    for (const QString& piece : target.pieces) {
      pieces.append(piece);
    }
    object.insert("pieces", pieces);
  }
  const bool has_colors = !target.colors.empty();
  if (has_colors) {
    object.insert("colors", ColorsToJson(target.colors));
  }
  return object;
}

CompTarget TargetFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  CompTarget target;
  target.layer = LayerId(std::max(object.value("layer").toInt(0), 0));
  const QJsonArray pieces =
      Bounded(object.value("pieces"), kMaxPieceTracks, "pieces", issues);
  for (const QJsonValue& piece : pieces) {
    target.pieces.push_back(piece.toString());
  }
  target.colors =
      ColorsFromJson(object.value("colors"), kMaxCompColors, issues);
  return target;
}

QJsonObject EffectToJson(const CompEffect& effect) {
  assert(effect.id.IsValid());
  QJsonObject object{
      {"id", effect.id.value()},
      {"kind", EnumToJson(static_cast<int>(effect.kind), kKindNames)},
      {"on", effect.is_on},
      {"blend", EnumToJson(static_cast<int>(effect.blend), kBlendNames)},
      {"target", TargetToJson(effect.target)},
      {"color", ColorToJson(effect.color)},
      {"style", effect.style},
      {"shot_light", effect.uses_shot_light},
      {"shape", EnumToJson(static_cast<int>(effect.shape), kShapeNames)}};
  QJsonArray stops;
  for (const GradientStop& stop : effect.stops) {
    stops.append(QJsonObject{{"at", stop.at},
                             {"color", ColorToJson(stop.color)}});
  }
  object.insert("stops", stops);
  QJsonArray swaps;
  for (const ColorSwap& swap : effect.swaps) {
    swaps.append(QJsonObject{{"from", ColorToJson(swap.from)},
                             {"to", ColorToJson(swap.to)}});
  }
  object.insert("swaps", swaps);
  QJsonObject knobs;
  for (size_t i = 0; i < effect.knobs.size(); ++i) {
    const bool has_keys = !effect.knobs[i].keys.empty();
    if (has_keys) {
      knobs.insert(kKnobNames[i], ChannelToJson(effect.knobs[i]));
    }
  }
  object.insert("knobs", knobs);
  return object;
}

CompEffect EffectFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  CompEffect effect;
  effect.id = CompEffectId(std::max(object.value("id").toInt(), 0));
  effect.kind = static_cast<CompKind>(
      EnumFromJson(object.value("kind"), kKindNames, issues));
  effect.is_on = object.value("on").toBool(true);
  effect.blend = static_cast<BlendMode>(
      EnumFromJson(object.value("blend"), kBlendNames, issues));
  effect.target = TargetFromJson(object.value("target").toObject(), issues);
  effect.color = ColorFromJson(object.value("color"), Qt::white, issues);
  effect.style = std::clamp(object.value("style").toInt(0), 0, 2);
  effect.uses_shot_light = object.value("shot_light").toBool(false);
  const bool has_shape = object.contains("shape");
  if (has_shape) {
    effect.shape = static_cast<GradientShape>(
        EnumFromJson(object.value("shape"), kShapeNames, issues));
  }
  const QJsonArray stops =
      Bounded(object.value("stops"), kMaxGradientStops, "stops", issues);
  for (const QJsonValue& item : stops) {
    const QJsonObject stop = item.toObject();
    effect.stops.push_back(GradientStop{
        std::clamp(stop.value("at").toDouble(0.0), 0.0, 1.0),
        ColorFromJson(stop.value("color"), Qt::black, issues)});
  }
  std::ranges::stable_sort(effect.stops, {}, &GradientStop::at);
  const QJsonArray swaps =
      Bounded(object.value("swaps"), kMaxCompColors, "swaps", issues);
  for (const QJsonValue& item : swaps) {
    const QJsonObject swap = item.toObject();
    effect.swaps.push_back(
        ColorSwap{ColorFromJson(swap.value("from"), Qt::black, issues),
                  ColorFromJson(swap.value("to"), Qt::white, issues)});
  }
  const QJsonObject knobs = object.value("knobs").toObject();
  for (size_t i = 0; i < effect.knobs.size(); ++i) {
    effect.knobs[i] = ChannelFromJson(knobs.value(kKnobNames[i]), 0.0, issues);
  }
  return effect;
}

}  // namespace

QJsonArray CompToJson(const std::vector<CompEffect>& comp) {
  assert(comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  QJsonArray array;
  for (const CompEffect& effect : comp) {
    array.append(EffectToJson(effect));
  }
  assert(array.size() == static_cast<qsizetype>(comp.size()));
  return array;
}

std::vector<CompEffect> CompFromJson(const QJsonValue& value,
                                     JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonArray array =
      Bounded(value, kMaxCompEffectsPerShot, "comp", issues);
  std::vector<CompEffect> comp;
  for (const QJsonValue& item : array) {
    CompEffect effect = EffectFromJson(item.toObject(), issues);
    // A missing or repeated id gets a fresh one, so each is told apart.
    const bool needs_id = !effect.id.IsValid() ||
                          FindCompEffect(comp, effect.id) != nullptr;
    if (needs_id) {
      effect.id = NextCompEffectId(comp);
    }
    comp.push_back(std::move(effect));
  }
  assert(comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  return comp;
}

}  // namespace snapper
