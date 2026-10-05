#include "io/doll_json.h"

#include <QJsonArray>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

QJsonArray Names(const std::vector<QString>& names) {
  assert(names.size() <= static_cast<size_t>(kMaxPieceDrawings));
  QJsonArray array;
  for (const QString& name : names) {
    array.append(name);
  }
  assert(array.size() == static_cast<qsizetype>(names.size()));
  return array;
}

// Reads an array, noting it when it is longer than limit.
QJsonArray Bounded(const QJsonValue& value, int limit, const char* what,
                   JsonIssues* issues) {
  assert(issues != nullptr);
  assert(limit > 0);
  const QJsonArray array = value.toArray();
  const bool is_too_long = array.size() > limit;
  if (is_too_long) {
    issues->Note(QStringLiteral("too many %1").arg(QLatin1String(what)));
    return QJsonArray();
  }
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
                          piece.value("rest").toDouble()});
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
