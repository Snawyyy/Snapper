#include "io/doll_json.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cassert>
#include <map>
#include <cmath>
#include <vector>

#include "io/collider_json.h"
#include "io/dot_group_json.h"
#include "io/point_motion_json.h"
#include "io/warp_json.h"
#include "model/node_physics.h"

namespace snapper {
namespace {

QJsonArray ReachToJson(const std::vector<double>& reach) {
  assert(reach.size() <= static_cast<size_t>(kMaxWarpPoints));
  QJsonArray array;
  for (const double cells : reach) {
    array.append(cells);
  }
  assert(array.size() == static_cast<qsizetype>(reach.size()));
  return array;
}

// One reach per point of grid, never negative; anything that doesn't
// fit the grid reads as no reach at all.
std::vector<double> ReachFromJson(const QJsonValue& value,
                                  const WarpGrid& grid) {
  assert(grid.columns >= 0 && grid.rows >= 0);
  const QJsonArray array = value.toArray();
  std::vector<double> reach;
  const bool is_fitting = array.size() == grid.PointCount();
  if (!is_fitting) {
    return reach;
  }
  for (const QJsonValue& cells : array) {
    const double read = cells.toDouble();
    reach.push_back(std::isfinite(read)
                        ? std::clamp(read, 0.0, double(kMaxWarpCells))
                        : 0.0);
  }
  assert(reach.size() <= static_cast<size_t>(kMaxWarpPoints));
  return reach;
}

// A node's kind; files from before jelly said "physics": true for cloth.
NodeKind KindFromJson(const QJsonObject& object) {
  assert(kNodeKindCount == 3);
  assert(object.size() < 1000000);
  const QString kind = object.value("kind").toString();
  const bool is_jelly = kind == "jelly";
  const bool is_cloth =
      kind == "cloth" || object.value("physics").toBool(false);
  return is_jelly ? NodeKind::kJelly
         : is_cloth ? NodeKind::kCloth
                    : NodeKind::kDrag;
}

// A setting kept 0 to 1, fallback when missing or not a number.
double UnitFromJson(const QJsonValue& number, double fallback) {
  assert(std::isfinite(fallback));
  assert(fallback >= 0.0 && fallback <= 1.0);
  const double read = number.toDouble(fallback);
  return std::isfinite(read) ? std::clamp(read, 0.0, 1.0) : fallback;
}

// A jelly node's knobs. Files from before the knobs kept jelly in the
// cloth settings (read already into read); those turn into the
// nearest knobs.
DragNode JellyFromJson(const QJsonObject& object, DragNode read) {
  assert(read.kind == NodeKind::kJelly);
  assert(object.size() < 1000000);
  const bool has_knobs = object.contains("wobble");
  if (!has_knobs) {
    return JellyFromOld(read);
  }
  read.spring = UnitFromJson(object.value("firmness"), read.spring);
  read.inertia = UnitFromJson(object.value("wobble"), read.inertia);
  read.stretch = UnitFromJson(object.value("squish"), read.stretch);
  read.gravity = UnitFromJson(object.value("droop"), read.gravity);
  return read;
}

QJsonArray DragToJson(const std::vector<DragNode>& nodes) {
  assert(nodes.size() <= static_cast<size_t>(kMaxWarpPoints));
  QJsonArray array;
  for (const DragNode& node : nodes) {
    QJsonObject object{{"point", node.point},
                       {"lag", node.lag},
                       {"bounce", node.bounce}};
    const bool is_jelly = node.kind == NodeKind::kJelly;
    const bool is_cloth = node.kind != NodeKind::kDrag && !is_jelly;
    if (is_jelly) {
      object.insert("kind", "jelly");
      object.insert("firmness", node.spring);
      object.insert("wobble", node.inertia);
      object.insert("squish", node.stretch);
      object.insert("droop", node.gravity);
    }
    if (is_cloth) {
      object.insert("kind", "cloth");
      object.insert("stretch", node.stretch);
      object.insert("inertia", node.inertia);
      object.insert("air", node.air);
      object.insert("gravity", node.gravity);
      object.insert("spring", node.spring);
    }
    array.append(object);
  }
  assert(array.size() == static_cast<qsizetype>(nodes.size()));
  return array;
}

// Drag nodes on points of grid, once each, settings kept 0 to 1.
std::vector<DragNode> DragFromJson(const QJsonValue& value,
                                   const WarpGrid& grid) {
  assert(grid.columns >= 0 && grid.rows >= 0);
  std::vector<DragNode> nodes;
  const QJsonArray array = value.toArray();
  for (const QJsonValue& item : array) {
    const QJsonObject object = item.toObject();
    const int point = object.value("point").toInt(-1);
    const bool is_new =
        point >= 0 && point < grid.PointCount() &&
        std::none_of(nodes.begin(), nodes.end(),
                     [point](const DragNode& n) { return n.point == point; });
    if (is_new) {
      const DragNode kDefault;
      nodes.push_back(
          {point, UnitFromJson(object.value("lag"), kDefault.lag),
           UnitFromJson(object.value("bounce"), kDefault.bounce),
           KindFromJson(object),
           UnitFromJson(object.value("stretch"), kDefault.stretch),
           UnitFromJson(object.value("inertia"), kDefault.inertia),
           UnitFromJson(object.value("air"), kDefault.air),
           UnitFromJson(object.value("gravity"), kDefault.gravity),
           UnitFromJson(object.value("spring"), kDefault.spring)});
      const bool is_jelly = nodes.back().kind == NodeKind::kJelly;
      if (is_jelly) {
        nodes.back() = JellyFromJson(object, nodes.back());
      }
    }
  }
  assert(nodes.size() <= static_cast<size_t>(kMaxWarpPoints));
  return nodes;
}

QJsonArray Names(const std::vector<QString>& names) {
  assert(names.size() <= static_cast<size_t>(kMaxPieceDrawings));
  QJsonArray array;
  for (const QString& name : names) {
    array.append(name);
  }
  assert(array.size() == static_cast<qsizetype>(names.size()));
  return array;
}

ArtPiece ArtPieceFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  ArtPiece piece;
  piece.name = object.value("name").toString();
  const QJsonArray drawings =
      Bounded(object.value("drawings"), kMaxPieceDrawings, "drawings", issues);
  for (const QJsonValue& drawing : drawings) {
    piece.drawings.push_back(drawing.toString());
  }
  piece.default_drawing = object.value("default").toInt();
  piece.position = PointFromJson(object.value("position"));
  piece.size = SizeFromJson(object.value("size"));
  // Older exports have no layer names; a list that does not match the
  // drawings one for one is dropped, so DrawingName falls back.
  const QJsonArray names = Bounded(object.value("layer_names"),
                                   kMaxPieceDrawings, "layer names", issues);
  if (names.size() == drawings.size()) {
    for (const QJsonValue& name : names) {
      piece.layer_names.push_back(name.toString());
    }
  }
  const QJsonArray folder = object.value("folder").toArray();
  for (const QJsonValue& group : folder) {
    piece.folder.push_back(group.toString());
  }
  const bool is_valid = !piece.name.isEmpty() && !piece.drawings.empty();
  if (!is_valid) {
    issues->Note(QStringLiteral("a piece has no name or drawings"));
  }
  assert(piece.drawings.size() <= static_cast<size_t>(kMaxPieceDrawings));
  return piece;
}

// Each layer's own dot settings, by the layer's id.
QJsonObject LayerDotsToJson(const std::map<LayerId, PieceDots>& layers) {
  assert(layers.size() < 1000000);
  QJsonObject out;
  for (const auto& [layer, dots] : layers) {
    out.insert(QString::number(layer.value()),
               QJsonObject{{"reach", ReachToJson(dots.warp_reach)},
                           {"drag", DragToJson(dots.drag_nodes)},
                           {"motions",
                            PointMotionsToJson(dots.point_motions)}});
  }
  assert(out.size() == static_cast<qsizetype>(layers.size()));
  return out;
}

std::map<LayerId, PieceDots> LayerDotsFromJson(const QJsonValue& value,
                                               const WarpGrid& grid,
                                               JsonIssues* issues) {
  assert(issues != nullptr);
  assert(grid.columns >= 0);
  std::map<LayerId, PieceDots> layers;
  const QJsonObject object = value.toObject();
  for (auto it = object.begin(); it != object.end(); ++it) {
    bool is_id = false;
    const int id = it.key().toInt(&is_id);
    const QJsonObject dots = it.value().toObject();
    const bool is_layer = is_id && id >= 1;
    if (is_layer) {
      layers[LayerId(id)] = {ReachFromJson(dots.value("reach"), grid),
                             DragFromJson(dots.value("drag"), grid),
                             PointMotionsFromJson(dots, grid, issues)};
    }
  }
  return layers;
}

}  // namespace

QJsonObject ArtToJson(const DollArt& art) {
  assert(art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  QJsonArray pieces;
  for (const ArtPiece& piece : art.pieces) {
    assert(!piece.name.isEmpty());
    QJsonObject object{{"name", piece.name},
                       {"drawings", Names(piece.drawings)},
                       {"default", piece.default_drawing},
                       {"position", PointToJson(piece.position)},
                       {"size", SizeToJson(piece.size)}};
    if (!piece.layer_names.empty()) {
      object.insert("layer_names", Names(piece.layer_names));
    }
    if (!piece.folder.empty()) {
      object.insert("folder", Names(piece.folder));
    }
    pieces.append(object);
  }
  return QJsonObject{{"canvas", SizeToJson(art.canvas)}, {"pieces", pieces}};
}

DollArt ArtFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  DollArt art;
  art.canvas = SizeFromJson(object.value("canvas"));
  const QJsonArray pieces =
      Bounded(object.value("pieces"), kMaxDollPieces, "pieces", issues);
  for (const QJsonValue& piece : pieces) {
    art.pieces.push_back(ArtPieceFromJson(piece.toObject(), issues));
  }
  assert(art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return art;
}

QJsonObject RigToJson(const Rig& rig, bool with_layers) {
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  QJsonArray pieces;
  for (const RigPiece& piece : rig.pieces) {
    QJsonObject object{
        {"name", piece.name},
        {"parent", piece.parent},
        {"pivot", PointToJson(piece.pivot)},
        {"order", piece.order},
        {"default", piece.default_drawing},
        {"rest", piece.rest_rotation},
        {"keep_shape", piece.keeps_shape},
        {"reach", ReachToJson(piece.warp_reach)},
        {"drag", DragToJson(piece.drag_nodes)},
        {"motions", PointMotionsToJson(piece.point_motions)},
        {"collider", ColliderToJson(piece.collider)}};
    WarpToJson(piece.warp, &object);
    const bool has_layers = with_layers && !piece.layer_dots.empty();
    if (has_layers) {
      object.insert("layers", LayerDotsToJson(piece.layer_dots));
    }
    pieces.append(object);
  }
  QJsonArray chains;
  for (const IkChain& chain : rig.chains) {
    chains.append(QJsonObject{{"name", chain.name},
                              {"upper", chain.upper},
                              {"lower", chain.lower},
                              {"tip", PointToJson(chain.tip)},
                              {"clockwise", chain.bends_clockwise}});
  }
  QJsonObject out{{"pieces", pieces},
                  {"chains", chains},
                  {"dot_groups", DotGroupsToJson(rig.dot_groups)}};
  if (!rig.krita_file.isEmpty()) {
    out.insert("krita_file", rig.krita_file);
  }
  return out;
}

Rig RigFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  Rig rig;
  const QJsonArray pieces =
      Bounded(object.value("pieces"), kMaxDollPieces, "rig pieces", issues);
  for (const QJsonValue& item : pieces) {
    const QJsonObject piece = item.toObject();
    const WarpGrid grid = WarpFromJson(piece, issues);
    rig.pieces.push_back({piece.value("name").toString(),
                          piece.value("parent").toString(),
                          PointFromJson(piece.value("pivot")),
                          piece.value("order").toInt(),
                          piece.value("default").toInt(-1), grid,
                          piece.value("rest").toDouble(),
                          piece.value("keep_shape").toBool(),
                          ReachFromJson(piece.value("reach"), grid),
                          DragFromJson(piece.value("drag"), grid),
                          PointMotionsFromJson(piece, grid, issues),
                          ColliderFromJson(piece.value("collider")),
                          LayerDotsFromJson(piece.value("layers"), grid,
                                            issues)});
  }
  const QJsonArray chains =
      Bounded(object.value("chains"), kMaxIkChains, "IK chains", issues);
  for (const QJsonValue& item : chains) {
    const QJsonObject chain = item.toObject();
    rig.chains.push_back({chain.value("name").toString(),
                          chain.value("upper").toString(),
                          chain.value("lower").toString(),
                          PointFromJson(chain.value("tip")),
                          chain.value("clockwise").toBool(true)});
  }
  rig.dot_groups = DotGroupsFromJson(object.value("dot_groups"), issues);
  rig.krita_file = object.value("krita_file").toString();
  assert(rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  return rig;
}

}  // namespace snapper
