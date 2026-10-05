#include "io/legacy_doll.h"

#include <QDir>
#include <QImageReader>
#include <QJsonArray>

#include <cassert>
#include <cmath>
#include <map>

namespace snapper {
namespace {

struct OldPiece final {
  int id = 0;
  int parent = 0;
  QPointF pin;
  QPointF pivot;
  ArtPiece art;
  RigPiece rig;
};

OldPiece ReadPiece(const QJsonObject& piece, const QDir& folder) {
  assert(!folder.path().isEmpty());
  assert(piece.contains("name"));
  OldPiece old;
  old.id = piece.value("id").toInt();
  old.parent = piece.value("parent").toInt();
  old.pin = QPointF(piece.value("pin_x").toDouble(),
                    piece.value("pin_y").toDouble());
  old.pivot = QPointF(piece.value("pivot_x").toDouble(),
                      piece.value("pivot_y").toDouble());
  old.art.name = piece.value("name").toString();
  const QJsonArray drawings = piece.value("drawings").toArray();
  for (const QJsonValue& drawing : drawings) {
    old.art.drawings.push_back(drawing.toString());
  }
  // The drawing knows its size; the first exporter pivoted at the
  // middle, so twice the pivot is the fallback.
  const QSize read =
      old.art.drawings.empty()
          ? QSize()
          : QImageReader(folder.filePath(old.art.drawings.front())).size();
  old.art.size = read.isValid()
                     ? read
                     : QSize(static_cast<int>(std::lround(old.pivot.x() * 2)),
                             static_cast<int>(std::lround(old.pivot.y() * 2)));
  old.rig.name = old.art.name;
  old.rig.pivot = old.pivot;
  old.rig.order = piece.value("order").toInt();
  old.rig.rest_rotation = piece.value("rest_rotation").toDouble();
  return old;
}

}  // namespace

LegacyDoll FromLegacy(const QJsonObject& object, const QString& folder) {
  assert(!folder.isEmpty());
  const QJsonArray list = object.value("pieces").toArray();
  std::map<int, OldPiece> pieces;
  const qsizetype count = std::min<qsizetype>(list.size(), kMaxDollPieces);
  for (qsizetype i = 0; i < count; ++i) {
    OldPiece old = ReadPiece(list.at(i).toObject(), QDir(folder));
    pieces[old.id] = std::move(old);
  }
  // A root's drawing sits so its pivot lands on its pin; a child's so
  // its pivot lands on the pin spot of its parent's drawing. Parents are
  // placed first; each pass places at least one piece of a tree.
  std::map<int, QPointF> placed;
  for (int pass = 0; pass <= kMaxDollPieces; ++pass) {
    for (const auto& [id, old] : pieces) {
      const bool has_parent = old.parent != 0 && pieces.contains(old.parent);
      const bool is_ready = !placed.contains(id) &&
                            (!has_parent || placed.contains(old.parent));
      if (is_ready) {
        const QPointF base = has_parent ? placed.at(old.parent) : QPointF();
        placed[id] = base + old.pin - old.pivot;
      }
    }
  }
  LegacyDoll doll;
  for (auto& [id, old] : pieces) {
    old.art.position = placed.contains(id) ? placed.at(id) : QPointF();
    const auto parent = pieces.find(old.parent);
    old.rig.parent =
        parent == pieces.end() ? QString() : parent->second.art.name;
    doll.art.pieces.push_back(old.art);
    doll.rig.pieces.push_back(old.rig);
  }
  assert(doll.art.pieces.size() == doll.rig.pieces.size());
  return doll;
}

}  // namespace snapper
