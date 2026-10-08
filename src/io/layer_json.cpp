#include "io/layer_json.h"

#include <QJsonArray>

#include <algorithm>
#include <array>
#include <cassert>
#include <variant>

namespace snapper {
namespace {

constexpr std::array<const char*, kTransitionKindCount> kTransitionNames = {
    "cut",        "swipe_left", "swipe_right", "swipe_up",
    "swipe_down", "flash",      "crossfade"};
constexpr std::array<const char*, kEffectKindCount> kEffectNames = {
    "flash", "fill", "invert", "zoom_punch", "halftone", "glitch",
    "posterize"};
// Index matches LayerContent's alternatives.
constexpr std::array<const char*, 4> kLayerTypes = {"doll", "image", "text",
                                                    "effect"};
static_assert(std::variant_size_v<LayerContent> == kLayerTypes.size());

// Each kind writes its own fields; ContentToJson picks by visiting.
void AddFields(const DollLayer& doll, QJsonObject* object) {
  assert(object != nullptr);
  assert(doll.pieces.size() <= static_cast<size_t>(kMaxPieceTracks));
  QJsonObject pieces;
  for (const auto& [name, channel] : doll.pieces) {
    pieces.insert(name, ChannelToJson(channel));
  }
  object->insert("doll", doll.doll);
  object->insert("flipped", doll.is_flipped);
  object->insert("pieces", pieces);
}

void AddFields(const ImageLayer& image, QJsonObject* object) {
  assert(object != nullptr);
  assert(object->contains("type"));
  object->insert("path", image.path);
}

void AddFields(const TextLayer& text, QJsonObject* object) {
  assert(object != nullptr);
  assert(object->contains("type"));
  object->insert("text", text.text);
  object->insert("font", text.font_family);
  object->insert("size", text.size);
  object->insert("bold", text.is_bold);
  object->insert("fill", ColorToJson(text.fill));
  object->insert("outline", ColorToJson(text.outline));
  object->insert("outline_width", text.outline_width);
}

void AddFields(const EffectLayer& effect, QJsonObject* object) {
  assert(object != nullptr);
  assert(static_cast<int>(effect.kind) < kEffectKindCount);
  object->insert("effect",
                 EnumToJson(static_cast<int>(effect.kind), kEffectNames));
  object->insert("color", ColorToJson(effect.color));
  object->insert("amount", ChannelToJson(effect.amount));
}

QJsonObject ContentToJson(const LayerContent& content) {
  assert(content.index() < kLayerTypes.size());
  QJsonObject object{
      {"type", QLatin1String(kLayerTypes[content.index()])}};
  std::visit([&object](const auto& kind) { AddFields(kind, &object); },
             content);
  assert(object.contains("type"));
  return object;
}

DollLayer DollFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  DollLayer doll;
  doll.doll = object.value("doll").toString();
  doll.is_flipped = object.value("flipped").toBool();
  const QJsonObject pieces = object.value("pieces").toObject();
  const bool is_too_many = pieces.size() > kMaxPieceTracks;
  if (is_too_many) {
    issues->Note(QStringLiteral("a doll layer has too many pieces"));
    return doll;
  }
  for (auto it = pieces.begin(); it != pieces.end(); ++it) {
    doll.pieces[it.key()] = ChannelFromJson(it.value(), PiecePose(), issues);
  }
  assert(doll.pieces.size() <= static_cast<size_t>(kMaxPieceTracks));
  return doll;
}

LayerContent ContentFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  const int type = EnumFromJson(object.value("type"), kLayerTypes, issues);
  assert(type >= 0 && type < static_cast<int>(kLayerTypes.size()));
  switch (type) {
    case 1:
      return ImageLayer{object.value("path").toString()};
    case 2: {
      TextLayer text;
      text.text = object.value("text").toString();
      text.font_family = object.value("font").toString(text.font_family);
      text.size = object.value("size").toDouble(text.size);
      text.is_bold = object.value("bold").toBool(text.is_bold);
      text.fill = ColorFromJson(object.value("fill"), text.fill, issues);
      text.outline =
          ColorFromJson(object.value("outline"), text.outline, issues);
      text.outline_width =
          object.value("outline_width").toDouble(text.outline_width);
      return text;
    }
    case 3: {
      EffectLayer effect;
      effect.kind = static_cast<EffectKind>(
          EnumFromJson(object.value("effect"), kEffectNames, issues));
      effect.color =
          ColorFromJson(object.value("color"), effect.color, issues);
      effect.amount = ChannelFromJson(object.value("amount"), 1.0, issues);
      return effect;
    }
    default:
      return DollFromJson(object, issues);
  }
}

QJsonObject LayerToJson(const Layer& layer) {
  assert(layer.id.IsValid());
  QJsonObject object = ContentToJson(layer.content);
  object.insert("id", layer.id.value());
  object.insert("name", layer.name);
  object.insert("start", layer.start.index());
  object.insert("length", layer.length.index());
  object.insert("visible", layer.is_visible);
  object.insert("transform", ChannelToJson(layer.transform));
  assert(object.contains("type"));
  return object;
}

Layer LayerFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  Layer layer;
  layer.id = LayerId(object.value("id").toInt());
  layer.name = object.value("name").toString();
  layer.start = Frame(object.value("start").toInt());
  layer.length = Frame(object.value("length").toInt());
  layer.is_visible = object.value("visible").toBool(true);
  layer.transform =
      ChannelFromJson(object.value("transform"), PiecePose(), issues);
  layer.content = ContentFromJson(object, issues);
  const bool is_valid = layer.id.IsValid();
  if (!is_valid) {
    issues->Note(QStringLiteral("a layer has no id"));
  }
  return layer;
}

QJsonObject EndToJson(const LinkEnd& end) {
  assert(end.layer.IsValid());
  assert(end.piece.size() < 100000);
  return QJsonObject{{"layer", end.layer.value()}, {"piece", end.piece}};
}

LinkEnd EndFromJson(const QJsonValue& value) {
  const QJsonObject object = value.toObject();
  assert(object.size() >= 0);
  return LinkEnd{LayerId(object.value("layer").toInt()),
                 object.value("piece").toString()};
}

QJsonArray LinksToJson(const std::vector<Link>& links) {
  assert(links.size() <= static_cast<size_t>(kMaxLinksPerShot));
  QJsonArray array;
  for (const Link& link : links) {
    array.append(QJsonObject{{"follower", EndToJson(link.follower)},
                             {"leader", EndToJson(link.leader)},
                             {"from", link.from.index()},
                             {"strength", link.strength}});
  }
  return array;
}

// Links whose layers are missing are dropped quietly, as editing does.
void ReadLinks(const QJsonArray& links, Shot* shot, JsonIssues* issues) {
  assert(shot != nullptr && issues != nullptr);
  const bool is_too_many = links.size() > kMaxLinksPerShot;
  if (is_too_many) {
    issues->Note(QStringLiteral("a shot has too many links"));
    return;
  }
  for (const QJsonValue& item : links) {
    const QJsonObject object = item.toObject();
    Link link;
    link.follower = EndFromJson(object.value("follower"));
    link.leader = EndFromJson(object.value("leader"));
    link.from = Frame(object.value("from").toInt());
    link.strength = std::clamp(object.value("strength").toDouble(1.0), 0.0,
                               kMaxLinkStrength);
    const bool is_repeat = FindLink(*shot, link.follower) != nullptr;
    if (!is_repeat) {
      shot->links.push_back(link);
    }
  }
  DropDeadLinks(shot);
}

}  // namespace

QJsonObject ShotToJson(const Shot& shot) {
  assert(shot.id.IsValid());
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QJsonArray layers;
  for (const Layer& layer : shot.layers) {
    layers.append(LayerToJson(layer));
  }
  return QJsonObject{
      {"id", shot.id.value()},
      {"name", shot.name},
      {"length", shot.length.index()},
      {"background", ColorToJson(shot.background)},
      {"camera", ChannelToJson(shot.camera)},
      {"transition",
       QJsonObject{{"kind", EnumToJson(static_cast<int>(shot.transition.kind),
                                       kTransitionNames)},
                   {"length", shot.transition.length.index()}}},
      {"layers", layers},
      {"links", LinksToJson(shot.links)}};
}

Shot ShotFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  Shot shot;
  shot.id = ShotId(object.value("id").toInt());
  shot.name = object.value("name").toString();
  shot.length = Frame(object.value("length").toInt(shot.length.index()));
  shot.background =
      ColorFromJson(object.value("background"), shot.background, issues);
  shot.camera = ChannelFromJson(object.value("camera"), CameraPose(), issues);
  const QJsonObject transition = object.value("transition").toObject();
  shot.transition.kind = static_cast<TransitionKind>(
      EnumFromJson(transition.value("kind"), kTransitionNames, issues));
  shot.transition.length = Frame(transition.value("length").toInt());
  const QJsonArray layers = object.value("layers").toArray();
  const bool is_too_many = layers.size() > kMaxLayersPerShot;
  if (is_too_many) {
    issues->Note(QStringLiteral("a shot has too many layers"));
    return shot;
  }
  for (const QJsonValue& layer : layers) {
    shot.layers.push_back(LayerFromJson(layer.toObject(), issues));
  }
  ReadLinks(object.value("links").toArray(), &shot, issues);
  return shot;
}

}  // namespace snapper
