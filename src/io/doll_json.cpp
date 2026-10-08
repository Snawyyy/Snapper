#include "io/doll_json.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "io/point_motion_json.h"

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
std::vector<double> ReachFromJson(const QJsonValue& value, WarpGrid grid) {
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

QJsonArray DragToJson(const std::vector<DragNode>& nodes) {
  assert(nodes.size() <= static_cast<size_t>(kMaxWarpPoints));
  QJsonArray array;
  for (const DragNode& node : nodes) {
    array.append(QJsonObject{{"point", node.point},
                             {"lag", node.lag},
                             {"bounce", node.bounce}});
  }
  assert(array.size() == static_cast<qsizetype>(nodes.size()));
  return array;
}

// Drag nodes on points of grid, once each, settings kept 0 to 1.
std::vector<DragNode> DragFromJson(const QJsonValue& value, WarpGrid grid) {
  assert(grid.columns >= 0 && grid.rows >= 0);
  std::vector<DragNode> nodes;
  const QJsonArray array = value.toArray();
  const auto unit = [](const QJsonValue& number, double fallback) {
    const double read = number.toDouble(fallback);
    return std::isfinite(read) ? std::clamp(read, 0.0, 1.0) : fallback;
  };
  for (const QJsonValue& item : array) {
    const QJsonObject object = item.toObject();
    const int point = object.value("point").toInt(-1);
    const bool is_new =
        point >= 0 && point < grid.PointCount() &&
        std::none_of(nodes.begin(), nodes.end(),
                     [point](const DragNode& n) { return n.point == point; });
    if (is_new) {
      nodes.push_back({point, unit(object.value("lag"), 0.5),
                       unit(object.value("bounce"), 0.5)});
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
  const bool is_valid = !piece.name.isEmpty() && !piece.drawings.empty();
  if (!is_valid) {
    issues->Note(QStringLiteral("a piece has no name or drawings"));
  }
  assert(piece.drawings.size() <= static_cast<size_t>(kMaxPieceDrawings));
  return piece;
}

}  // namespace

QJsonObject ArtToJson(const DollArt& art) {
  assert(art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  QJsonArray pieces;
  for (const ArtPiece& piece : art.pieces) {
    assert(!piece.name.isEmpty());
    pieces.append(QJsonObject{{"name", piece.name},
                              {"drawings", Names(piece.drawings)},
                              {"default", piece.default_drawing},
                              {"position", PointToJson(piece.position)},
                              {"size", SizeToJson(piece.size)}});
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

QJsonObject RigToJson(const Rig& rig) {
  assert(rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  QJsonArray pieces;
  for (const RigPiece& piece : rig.pieces) {
    pieces.append(QJsonObject{
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
        {"warp", QJsonArray{piece.warp.columns, piece.warp.rows}}});
  }
  QJsonArray chains;
  for (const IkChain& chain : rig.chains) {
    chains.append(QJsonObject{{"name", chain.name},
                              {"upper", chain.upper},
                              {"lower", chain.lower},
                              {"tip", PointToJson(chain.tip)},
                              {"clockwise", chain.bends_clockwise}});
  }
  return QJsonObject{{"pieces", pieces}, {"chains", chains}};
}

Rig RigFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  Rig rig;
  const QJsonArray pieces =
      Bounded(object.value("pieces"), kMaxDollPieces, "rig pieces", issues);
  for (const QJsonValue& item : pieces) {
    const QJsonObject piece = item.toObject();
    const QJsonArray warp = piece.value("warp").toArray();
    const WarpGrid grid{std::clamp(warp.at(0).toInt(), 0, kMaxWarpCells),
                        std::clamp(warp.at(1).toInt(), 0, kMaxWarpCells)};
    rig.pieces.push_back({piece.value("name").toString(),
                          piece.value("parent").toString(),
                          PointFromJson(piece.value("pivot")),
                          piece.value("order").toInt(),
                          piece.value("default").toInt(-1), grid,
                          piece.value("rest").toDouble(),
                          piece.value("keep_shape").toBool(),
                          ReachFromJson(piece.value("reach"), grid),
                          DragFromJson(piece.value("drag"), grid),
                          PointMotionsFromJson(piece, grid, issues)});
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
  assert(rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  return rig;
}

}  // namespace snapper
