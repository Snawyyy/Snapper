#include "io/character_json.h"

#include <QJsonArray>

#include <algorithm>
#include <array>
#include <cassert>

namespace snapper {
namespace {

// Index matches LineBrush.
constexpr std::array<const char*, kLineBrushCount> kBrushNames = {
    "solid", "pencil", "ink", "marker", "chalk", "dots"};

constexpr std::array<const char*, kPartKindCount> kPartKindNames = {
    "group", "bone", "vector", "switch", "image"};
constexpr std::array<const char*, kBindModeCount> kBindNames = {
    "flexi", "points", "layer", "none"};

double Number(const QJsonObject& object, const char* key, double fallback) {
  assert(key != nullptr);
  assert(object.size() >= 0);
  const QJsonValue value = object.value(QLatin1String(key));
  return value.isDouble() ? value.toDouble() : fallback;
}

QJsonObject PointToJson(const VectorPoint& point) {
  assert(point.id.IsValid());
  assert(point.width >= 0.0);
  QJsonObject object{{"id", point.id.value()},
                     {"at", snapper::PointToJson(point.at)}};
  if (point.curvature != kSmoothCurvature) {
    object.insert("c", point.curvature);
  }
  if (point.width != 1.0) {
    object.insert("w", point.width);
  }
  return object;
}

VectorPoint PointFromJson(const QJsonObject& object) {
  assert(object.size() >= 0);
  VectorPoint point;
  point.id = PointId(object.value("id").toInt());
  point.at = snapper::PointFromJson(object.value("at"));
  point.curvature =
      std::clamp(Number(object, "c", kSmoothCurvature), 0.0, kMaxCurvature);
  point.width = std::clamp(Number(object, "w", 1.0), 0.0, 10.0);
  assert(point.curvature >= 0.0);
  return point;
}

// A shape's look: fill, line, width, brush and its style, if any. Its
// curves are written by the drawing.
QJsonObject ShapeToJson(const VectorShape& shape) {
  assert(shape.stroke_width >= 0.0);
  assert(static_cast<int>(shape.brush) < kLineBrushCount);
  QJsonObject object{
      {"fill", ColorToJson(shape.fill)},
      {"has_fill", shape.has_fill},
      {"stroke", ColorToJson(shape.stroke)},
      {"has_stroke", shape.has_stroke},
      {"width", shape.stroke_width},
      {"brush", EnumToJson(static_cast<int>(shape.brush), kBrushNames)}};
  if (shape.style.IsValid()) {
    object.insert("style", shape.style.value());
  }
  return object;
}

QJsonObject DrawingToJson(const VectorDrawing& drawing) {
  assert(IsSound(drawing));
  QJsonArray points;
  for (const VectorPoint& point : drawing.points) {
    points.append(PointToJson(point));
  }
  QJsonArray curves;
  for (const VectorCurve& curve : drawing.curves) {
    QJsonArray ids;
    for (PointId id : curve.points) {
      ids.append(id.value());
    }
    curves.append(QJsonObject{{"points", ids}, {"closed", curve.is_closed}});
  }
  QJsonArray shapes;
  for (const VectorShape& shape : drawing.shapes) {
    QJsonArray indices;
    for (int curve : shape.curves) {
      indices.append(curve);
    }
    QJsonObject object = ShapeToJson(shape);
    object.insert("curves", indices);
    shapes.append(object);
  }
  assert(points.size() == static_cast<qsizetype>(drawing.points.size()));
  return QJsonObject{{"points", points}, {"curves", curves}, {"shapes", shapes}};
}

VectorCurve CurveFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  VectorCurve curve;
  curve.is_closed = object.value("closed").toBool();
  const QJsonArray ids =
      Bounded(object.value("points"), kMaxCurvePoints, "curve points", issues);
  for (const QJsonValue& id : ids) {
    curve.points.push_back(PointId(id.toInt()));
  }
  return curve;
}

VectorShape ShapeFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  VectorShape shape;
  const QJsonArray curves = Bounded(object.value("curves"), kMaxVectorCurves,
                                    "shape curves", issues);
  for (const QJsonValue& curve : curves) {
    shape.curves.push_back(curve.toInt());
  }
  shape.fill = ColorFromJson(object.value("fill"), shape.fill, issues);
  shape.has_fill = object.value("has_fill").toBool(true);
  shape.stroke = ColorFromJson(object.value("stroke"), shape.stroke, issues);
  shape.has_stroke = object.value("has_stroke").toBool(true);
  shape.stroke_width =
      std::clamp(Number(object, "width", shape.stroke_width), 0.0,
                 static_cast<double>(kMaxStrokeWidth));
  shape.style = StyleId(std::max(0, object.value("style").toInt()));
  // Files from before brushes have none: solid.
  const bool has_brush = object.contains("brush");
  if (has_brush) {
    shape.brush = static_cast<LineBrush>(
        EnumFromJson(object.value("brush"), kBrushNames, issues));
  }
  return shape;
}

VectorDrawing DrawingFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  VectorDrawing drawing;
  for (const QJsonValue& item : Bounded(object.value("points"),
                                        kMaxVectorPoints, "points", issues)) {
    drawing.points.push_back(PointFromJson(item.toObject()));
  }
  for (const QJsonValue& item : Bounded(object.value("curves"),
                                        kMaxVectorCurves, "curves", issues)) {
    drawing.curves.push_back(CurveFromJson(item.toObject(), issues));
  }
  for (const QJsonValue& item : Bounded(object.value("shapes"),
                                        kMaxVectorShapes, "shapes", issues)) {
    drawing.shapes.push_back(ShapeFromJson(item.toObject(), issues));
  }
  return drawing;
}

QJsonObject BoneToJson(const Bone& bone) {
  assert(bone.id.IsValid());
  assert(bone.length >= 0.0);
  return QJsonObject{{"id", bone.id.value()},
                     {"name", bone.name},
                     {"parent", bone.parent.value()},
                     {"base", snapper::PointToJson(bone.base)},
                     {"angle", bone.angle},
                     {"length", bone.length},
                     {"strength", bone.strength},
                     {"has_limits", bone.has_limits},
                     {"min", bone.min_turn},
                     {"max", bone.max_turn},
                     {"smart", bone.is_smart},
                     {"locked", bone.is_locked},
                     {"dynamics", bone.dynamics}};
}

Bone BoneFromJson(const QJsonObject& object) {
  assert(object.size() >= 0);
  Bone bone;
  bone.id = BoneId(object.value("id").toInt());
  bone.name = object.value("name").toString();
  bone.parent = BoneId(std::max(0, object.value("parent").toInt()));
  bone.base = snapper::PointFromJson(object.value("base"));
  bone.angle = Number(object, "angle", 0.0);
  bone.length = std::clamp(Number(object, "length", bone.length), 0.0, 1e5);
  bone.strength =
      std::clamp(Number(object, "strength", bone.strength), 0.0, 1e5);
  bone.has_limits = object.value("has_limits").toBool();
  bone.min_turn = Number(object, "min", bone.min_turn);
  bone.max_turn = Number(object, "max", bone.max_turn);
  bone.is_smart = object.value("smart").toBool();
  bone.is_locked = object.value("locked").toBool();
  bone.dynamics = std::clamp(Number(object, "dynamics", 0.0), 0.0, 1.0);
  assert(bone.length >= 0.0);
  return bone;
}

QJsonObject PartToJson(const CharacterPart& part) {
  assert(part.id.IsValid());
  assert(static_cast<int>(part.kind) < kPartKindCount);
  QJsonObject object{
      {"id", part.id.value()},
      {"name", part.name},
      {"kind", EnumToJson(static_cast<int>(part.kind), kPartKindNames)},
      {"parent", part.parent.value()},
      {"origin", snapper::PointToJson(part.origin)},
      {"offset", snapper::PointToJson(part.offset)},
      {"rotation", part.rotation},
      {"sx", part.scale_x},
      {"sy", part.scale_y},
      {"opacity", part.opacity},
      {"visible", part.is_visible},
      {"flipped", part.is_flipped},
      {"bind", EnumToJson(static_cast<int>(part.bind), kBindNames)},
      {"bone", part.bound_bone.value()}};
  QJsonObject point_bones;
  for (const auto& [point, bone] : part.point_bones) {
    point_bones.insert(QString::number(point.value()), bone.value());
  }
  object.insert("point_bones", point_bones);
  if (part.kind == PartKind::kVector) {
    object.insert("drawing", DrawingToJson(part.drawing));
  }
  QJsonArray bones;
  for (const Bone& bone : part.bones) {
    bones.append(BoneToJson(bone));
  }
  object.insert("bones", bones);
  object.insert("rest_pick", part.rest_pick.value());
  object.insert("image", part.image);
  object.insert("image_size", QJsonArray{part.image_size.width(),
                                         part.image_size.height()});
  return object;
}

void ReadPartLook(const QJsonObject& object, CharacterPart* part) {
  assert(part != nullptr);
  assert(object.size() >= 0);
  part->origin = snapper::PointFromJson(object.value("origin"));
  part->offset = snapper::PointFromJson(object.value("offset"));
  part->rotation = Number(object, "rotation", 0.0);
  part->scale_x = Number(object, "sx", 1.0);
  part->scale_y = Number(object, "sy", 1.0);
  part->opacity = std::clamp(Number(object, "opacity", 1.0), 0.0, 1.0);
  part->is_visible = object.value("visible").toBool(true);
  part->is_flipped = object.value("flipped").toBool();
  const QJsonArray size = object.value("image_size").toArray();
  part->image_size = QSizeF(size.at(0).toDouble(), size.at(1).toDouble());
  part->image = object.value("image").toString();
}

CharacterPart PartFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  CharacterPart part;
  part.id = PartId(object.value("id").toInt());
  part.name = object.value("name").toString();
  part.kind = static_cast<PartKind>(
      EnumFromJson(object.value("kind"), kPartKindNames, issues));
  part.parent = PartId(std::max(0, object.value("parent").toInt()));
  ReadPartLook(object, &part);
  part.bind = static_cast<BindMode>(
      EnumFromJson(object.value("bind"), kBindNames, issues));
  part.bound_bone = BoneId(std::max(0, object.value("bone").toInt()));
  const QJsonObject point_bones = object.value("point_bones").toObject();
  const bool is_too_many = point_bones.size() > kMaxVectorPoints;
  if (is_too_many) {
    issues->Note(QStringLiteral("a part binds too many points"));
  }
  for (auto it = point_bones.begin(); !is_too_many && it != point_bones.end();
       ++it) {
    part.point_bones[PointId(it.key().toInt())] = BoneId(it.value().toInt());
  }
  part.drawing = DrawingFromJson(object.value("drawing").toObject(), issues);
  for (const QJsonValue& bone : Bounded(object.value("bones"),
                                        kMaxCharacterBones, "bones", issues)) {
    part.bones.push_back(BoneFromJson(bone.toObject()));
  }
  part.rest_pick = PartId(std::max(0, object.value("rest_pick").toInt()));
  return part;
}

QJsonObject PartPoseToJson(const PartPose& pose) {
  assert(pose.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(pose.opacity >= 0.0);
  QJsonObject object;
  const PartPose rest;
  if (pose.offset != rest.offset) {
    object.insert("x", pose.offset.x());
    object.insert("y", pose.offset.y());
  }
  if (pose.rotation != rest.rotation) object.insert("r", pose.rotation);
  if (pose.scale_x != rest.scale_x) object.insert("sx", pose.scale_x);
  if (pose.scale_y != rest.scale_y) object.insert("sy", pose.scale_y);
  if (pose.opacity != rest.opacity) object.insert("o", pose.opacity);
  if (pose.order != rest.order) object.insert("z", pose.order);
  if (pose.pick.IsValid()) object.insert("pick", pose.pick.value());
  if (!pose.points.empty()) {
    QJsonObject points;
    for (const auto& [id, at] : pose.points) {
      points.insert(QString::number(id.value()), snapper::PointToJson(at));
    }
    object.insert("points", points);
  }
  return object;
}

PartPose PartPoseFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  PartPose pose;
  pose.offset = QPointF(Number(object, "x", 0.0), Number(object, "y", 0.0));
  pose.rotation = Number(object, "r", 0.0);
  pose.scale_x = Number(object, "sx", 1.0);
  pose.scale_y = Number(object, "sy", 1.0);
  pose.opacity = std::clamp(Number(object, "o", 1.0), 0.0, 1.0);
  pose.order = std::clamp(object.value("z").toInt(), -kMaxCharacterParts,
                          kMaxCharacterParts);
  pose.pick = PartId(std::max(0, object.value("pick").toInt()));
  const QJsonObject points = object.value("points").toObject();
  if (points.size() > kMaxVectorPoints) {
    issues->Note(QStringLiteral("a pose moves too many points"));
    return pose;
  }
  for (auto it = points.begin(); it != points.end(); ++it) {
    pose.points[PointId(it.key().toInt())] =
        snapper::PointFromJson(it.value());
  }
  return pose;
}

QJsonObject BonePoseToJson(const BonePose& pose) {
  assert(pose.scale > -1e9);
  assert(pose.turn > -1e9);
  QJsonObject object;
  if (pose.turn != 0.0) object.insert("t", pose.turn);
  if (pose.offset != QPointF()) {
    object.insert("x", pose.offset.x());
    object.insert("y", pose.offset.y());
  }
  if (pose.scale != 1.0) object.insert("s", pose.scale);
  return object;
}

BonePose BonePoseFromJson(const QJsonObject& object) {
  assert(object.size() >= 0);
  BonePose pose;
  pose.turn = Number(object, "t", 0.0);
  pose.offset = QPointF(Number(object, "x", 0.0), Number(object, "y", 0.0));
  pose.scale = Number(object, "s", 1.0);
  assert(pose.scale > -1e9);
  return pose;
}

QJsonObject ActionToJson(const SmartAction& action) {
  assert(action.bone.IsValid());
  assert(action.stops.size() <= static_cast<size_t>(kMaxActionStops));
  QJsonArray stops;
  for (const ActionStop& stop : action.stops) {
    stops.append(
        QJsonObject{{"turn", stop.turn}, {"pose", ValueToJson(stop.pose)}});
  }
  return QJsonObject{
      {"bone", action.bone.value()}, {"name", action.name}, {"stops", stops}};
}

SmartAction ActionFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  SmartAction action;
  action.bone = BoneId(object.value("bone").toInt());
  action.name = object.value("name").toString();
  for (const QJsonValue& item : Bounded(object.value("stops"),
                                        kMaxActionStops, "stops", issues)) {
    const QJsonObject stop = item.toObject();
    action.stops.push_back(
        {Number(stop, "turn", 0.0),
         ValueFromJson(stop.value("pose"), CharacterPose{}, issues)});
  }
  std::ranges::sort(action.stops, {}, &ActionStop::turn);
  return action;
}

}  // namespace

QJsonValue ValueToJson(const CharacterPose& pose) {
  assert(pose.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(pose.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  QJsonObject object;
  const QJsonObject root = PartPoseToJson(pose.root);
  if (!root.isEmpty()) {
    object.insert("root", root);
  }
  QJsonObject parts;
  for (const auto& [id, part] : pose.parts) {
    const QJsonObject json = PartPoseToJson(part);
    if (!json.isEmpty()) {
      parts.insert(QString::number(id.value()), json);
    }
  }
  if (!parts.isEmpty()) {
    object.insert("parts", parts);
  }
  QJsonObject bones;
  for (const auto& [id, bone] : pose.bones) {
    const QJsonObject json = BonePoseToJson(bone);
    if (!json.isEmpty()) {
      bones.insert(QString::number(id.value()), json);
    }
  }
  if (!bones.isEmpty()) {
    object.insert("bones", bones);
  }
  return object;
}

CharacterPose ValueFromJson(const QJsonValue& value, CharacterPose fallback,
                            JsonIssues* issues) {
  assert(issues != nullptr);
  if (!value.isObject()) {
    return fallback;
  }
  const QJsonObject object = value.toObject();
  CharacterPose pose;
  pose.root = PartPoseFromJson(object.value("root").toObject(), issues);
  const QJsonObject parts = object.value("parts").toObject();
  const QJsonObject bones = object.value("bones").toObject();
  const bool is_too_many = parts.size() > kMaxCharacterParts ||
                           bones.size() > kMaxCharacterBones;
  if (is_too_many) {
    issues->Note(QStringLiteral("a pose has too many parts or bones"));
    return pose;
  }
  for (auto it = parts.begin(); it != parts.end(); ++it) {
    pose.parts[PartId(it.key().toInt())] =
        PartPoseFromJson(it.value().toObject(), issues);
  }
  for (auto it = bones.begin(); it != bones.end(); ++it) {
    pose.bones[BoneId(it.key().toInt())] =
        BonePoseFromJson(it.value().toObject());
  }
  return pose;
}

QJsonObject CharacterToJson(const Character& character) {
  assert(IsSound(character));
  assert(character.next_id >= 1);
  QJsonArray parts;
  for (const CharacterPart& part : character.parts) {
    parts.append(PartToJson(part));
  }
  QJsonArray actions;
  for (const SmartAction& action : character.actions) {
    actions.append(ActionToJson(action));
  }
  QJsonArray styles;
  for (const VectorStyle& style : character.styles) {
    QJsonObject look = ShapeToJson(style.look);
    look.insert("id", style.id.value());
    look.insert("name", style.name);
    styles.append(look);
  }
  return QJsonObject{{"name", character.name},
                     {"next_id", character.next_id},
                     {"parts", parts},
                     {"actions", actions},
                     {"styles", styles}};
}

Character CharacterFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  Character character;
  character.name = object.value("name").toString();
  character.next_id = std::max(1, object.value("next_id").toInt(1));
  for (const QJsonValue& item : Bounded(object.value("parts"),
                                        kMaxCharacterParts, "parts", issues)) {
    character.parts.push_back(PartFromJson(item.toObject(), issues));
  }
  for (const QJsonValue& item :
       Bounded(object.value("actions"), kMaxSmartActions, "actions", issues)) {
    character.actions.push_back(ActionFromJson(item.toObject(), issues));
  }
  for (const QJsonValue& item : Bounded(object.value("styles"),
                                        kMaxCharacterStyles, "styles", issues)) {
    const QJsonObject read = item.toObject();
    VectorShape look = ShapeFromJson(read, issues);
    look.style = StyleId();
    character.styles.push_back(
        {StyleId(read.value("id").toInt()), read.value("name").toString(),
         look});
  }
  if (!IsSound(character)) {
    issues->Note(QStringLiteral("the character's parts do not form a tree"));
    return Character{.name = character.name};
  }
  return character;
}

}  // namespace snapper
