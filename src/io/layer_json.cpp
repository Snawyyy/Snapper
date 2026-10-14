#include "io/layer_json.h"

#include <QJsonArray>

#include <algorithm>
#include <array>
#include <cassert>
#include <variant>

#include "io/dot_group_json.h"
#include "io/comp_json.h"
#include "io/reference_json.h"
#include "io/storyboard_json.h"
#include "model/doll.h"
#include "model/smart_dial.h"

namespace snapper {
namespace {

constexpr std::array<const char*, kEffectKindCount> kEffectNames = {
    "flash", "fill", "invert", "zoom_punch", "halftone", "glitch",
    "posterize"};
// Index matches LayerContent's alternatives.
constexpr std::array<const char*, 5> kLayerTypes = {
    "doll", "image", "text", "effect", "storyboard"};
static_assert(std::variant_size_v<LayerContent> == kLayerTypes.size());
// PlayMode by its value.
constexpr std::array<const char*, kPlayModeCount> kPlayModes = {
    "loop", "once", "ping_pong", "hold"};

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
  const bool has_dials = !doll.dials.empty();
  if (has_dials) {
    QJsonObject dials;
    for (const auto& [name, channel] : doll.dials) {
      dials.insert(name, ChannelToJson(channel));
    }
    object->insert("dials", dials);
  }
  const bool has_skins = !doll.skins.keys.empty();
  if (has_skins) {
    object->insert("skins", ChannelToJson(doll.skins));
  }
  const bool has_play = doll.play != ObjectPlay();
  if (has_play) {
    object->insert(
        "play",
        QJsonObject{
            {"mode", EnumToJson(static_cast<int>(doll.play.mode), kPlayModes)},
            {"frames_per_drawing", doll.play.frames_per_drawing},
            {"start", doll.play.start.index()},
            {"offset", doll.play.offset}});
  }
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
  AddLetters(text, object);
}

void AddFields(const EffectLayer& effect, QJsonObject* object) {
  assert(object != nullptr);
  assert(static_cast<int>(effect.kind) < kEffectKindCount);
  object->insert("effect",
                 EnumToJson(static_cast<int>(effect.kind), kEffectNames));
  object->insert("color", ColorToJson(effect.color));
  object->insert("amount", ChannelToJson(effect.amount));
}

void AddFields(const StoryboardLayer& board, QJsonObject* object) {
  assert(object != nullptr);
  assert(object->contains("type"));
  AddStoryboardFields(board, object);
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
  const QJsonObject dials = object.value("dials").toObject();
  const bool is_dials_ok = dials.size() <= kMaxSmartDials;
  if (!is_dials_ok) {
    issues->Note(QStringLiteral("a doll layer has too many dials"));
    return doll;
  }
  for (auto it = dials.begin(); it != dials.end(); ++it) {
    doll.dials[it.key()] = ChannelFromJson(it.value(), 0.5, issues);
  }
  doll.skins = ChannelFromJson(object.value("skins"), SkinSet(), issues);
  const bool has_play = object.value("play").isObject();
  if (has_play) {
    const QJsonObject play = object.value("play").toObject();
    doll.play.mode = static_cast<PlayMode>(
        EnumFromJson(play.value("mode"), kPlayModes, issues));
    doll.play.frames_per_drawing =
        std::clamp(play.value("frames_per_drawing").toInt(), 0,
                   kMaxFramesPerDrawing);
    doll.play.start = Frame(play.value("start").toInt());
    doll.play.offset =
        std::clamp(play.value("offset").toInt(), 0, kMaxObjectFrames * 2);
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
      ReadLetters(object, &text, issues);
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
    case 4:
      return StoryboardFromJson(object, issues);
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
  assert(layer.name == object.value("name").toString());
  return layer;
}

QJsonObject EndToJson(const LinkEnd& end) {
  assert(end.layer.IsValid());
  assert(end.piece.size() < 100000);
  return QJsonObject{{"layer", end.layer.value()}, {"piece", end.piece}};
}

LinkEnd EndFromJson(const QJsonValue& value) {
  const QJsonObject object = value.toObject();
  const int layer = object.value("layer").toInt();
  // A bad id becomes no layer, so DropDeadLinks clears the link.
  const LinkEnd end{LayerId(std::max(layer, 0)),
                    object.value("piece").toString()};
  assert(end.layer.value() >= 0);
  assert(end.layer.IsValid() == (layer > 0));
  return end;
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
  assert(array.size() == static_cast<qsizetype>(links.size()));
  return array;
}

// Links whose layers are missing are dropped quietly, as editing does.
void ReadLinks(const QJsonArray& links, Shot* shot, JsonIssues* issues) {
  assert(shot != nullptr);
  assert(issues != nullptr);
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

QJsonArray HoldsToJson(const std::vector<Hold>& holds) {
  assert(holds.size() <= static_cast<size_t>(kMaxHoldsPerShot));
  QJsonArray array;
  for (const Hold& hold : holds) {
    QJsonObject object{{"object", hold.object.value()},
                       {"at", hold.at.index()}};
    const bool has_holder = !hold.IsDrop();
    if (has_holder) {
      object.insert("holder", EndToJson(hold.holder));
    }
    array.append(object);
  }
  assert(array.size() == static_cast<qsizetype>(holds.size()));
  return array;
}

// Holds whose object is missing are dropped quietly; a missing holder
// reads as a drop (DropDeadLinks).
void ReadHolds(const QJsonValue& value, Shot* shot, JsonIssues* issues) {
  assert(shot != nullptr);
  assert(issues != nullptr);
  const QJsonArray holds =
      Bounded(value, kMaxHoldsPerShot, "holds", issues);
  for (const QJsonValue& item : holds) {
    const QJsonObject object = item.toObject();
    Hold hold;
    hold.object = LayerId(std::max(object.value("object").toInt(), 0));
    hold.at = Frame(object.value("at").toInt());
    const bool has_holder = object.contains("holder");
    if (has_holder) {
      hold.holder = EndFromJson(object.value("holder"));
    }
    const bool is_repeat = std::ranges::any_of(
        shot->holds, [&hold](const Hold& other) {
          return other.object == hold.object && other.at == hold.at;
        });
    if (!is_repeat) {
      shot->holds.push_back(hold);
    }
  }
  DropDeadLinks(shot);
}

}  // namespace

void AddLetters(const TextLayer& text, QJsonObject* object) {
  assert(object != nullptr);
  assert(text.size > 0.0);
  object->insert("font", text.font_family);
  object->insert("size", text.size);
  object->insert("bold", text.is_bold);
  object->insert("fill", ColorToJson(text.fill));
  object->insert("outline", ColorToJson(text.outline));
  object->insert("outline_width", text.outline_width);
}

void ReadLetters(const QJsonObject& object, TextLayer* text,
                 JsonIssues* issues) {
  assert(text != nullptr);
  assert(issues != nullptr);
  text->font_family = object.value("font").toString(text->font_family);
  text->size = std::max(object.value("size").toDouble(text->size), 1.0);
  text->is_bold = object.value("bold").toBool(text->is_bold);
  text->fill = ColorFromJson(object.value("fill"), text->fill, issues);
  text->outline =
      ColorFromJson(object.value("outline"), text->outline, issues);
  text->outline_width =
      std::max(object.value("outline_width").toDouble(text->outline_width),
               0.0);
}

QJsonObject ShotToJson(const Shot& shot) {
  assert(shot.id.IsValid());
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QJsonArray layers;
  for (const Layer& layer : shot.layers) {
    layers.append(LayerToJson(layer));
  }
  QJsonObject object{{"id", shot.id.value()},
                     {"name", shot.name},
                     {"length", shot.length.index()},
                     {"background", ColorToJson(shot.background)},
                     {"camera", ChannelToJson(shot.camera)},
                     {"layers", layers},
                     {"links", LinksToJson(shot.links)},
                     {"dot_groups", DotGroupsToJson(shot.dot_groups)}};
  const bool has_holds = !shot.holds.empty();
  if (has_holds) {
    object.insert("holds", HoldsToJson(shot.holds));
  }
  const bool is_transparent = shot.is_transparent;
  if (is_transparent) {
    object.insert("transparent", true);
  }
  const bool has_references = !shot.references.empty();
  if (has_references) {
    object.insert("references", ReferencesToJson(shot.references));
  }
  const bool has_comp = !shot.comp.empty();
  if (has_comp) {
    object.insert("comp", CompToJson(shot.comp));
  }
  const bool has_light = !shot.light.keys.empty();
  if (has_light) {
    object.insert("light", ChannelToJson(shot.light));
  }
  return object;
}

Shot ShotFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  Shot shot;
  shot.id = ShotId(object.value("id").toInt());
  shot.name = object.value("name").toString();
  shot.length = Frame(object.value("length").toInt(shot.length.index()));
  shot.background =
      ColorFromJson(object.value("background"), shot.background, issues);
  shot.is_transparent = object.value("transparent").toBool(false);
  shot.camera = ChannelFromJson(object.value("camera"), CameraPose(), issues);
  // A "transition" left from before transitions moved to the reel is
  // read by MoveShotTransitions.
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
  shot.dot_groups = DotGroupsFromJson(object.value("dot_groups"), issues);
  ReadHolds(object.value("holds"), &shot, issues);
  shot.references = ReferencesFromJson(object.value("references"), issues);
  shot.comp = CompFromJson(object.value("comp"), issues);
  shot.light = ChannelFromJson(object.value("light"), 0.0, issues);
  assert(static_cast<int>(shot.layers.size()) <= kMaxLayersPerShot);
  return shot;
}

}  // namespace snapper
