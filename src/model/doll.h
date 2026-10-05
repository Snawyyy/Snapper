#ifndef SNAPPER_MODEL_DOLL_H_
#define SNAPPER_MODEL_DOLL_H_

#include <QPointF>
#include <QSize>
#include <QString>

#include <vector>

namespace snapper {

constexpr int kMaxDollPieces = 128;
constexpr int kMaxPieceDrawings = 64;
constexpr int kMaxIkChains = 32;
constexpr int kMaxWarpCells = 8;

// One piece as Krita drew it. Doll space has its origin at the middle
// of the Krita canvas, y down, in pixels.
struct ArtPiece final {
  QString name;
  // PNG names inside the doll folder. All are the same size.
  std::vector<QString> drawings;
  int default_drawing = 0;
  // Top-left corner of the drawings in doll space, and their size.
  QPointF position;
  QSize size;

  bool operator==(const ArtPiece&) const = default;
};

// What Krita exported: art.json. Snapper only reads it.
struct DollArt final {
  QSize canvas;
  // Bottom to top, as Krita stacks them.
  std::vector<ArtPiece> pieces;

  bool operator==(const DollArt&) const = default;
};

// 0 by 0 is off. Otherwise the drawing is cut into columns by rows
// cells whose corners can be pushed.
struct WarpGrid final {
  int columns = 0;
  int rows = 0;

  bool IsOn() const { return columns > 0 && rows > 0; }
  int PointCount() const { return IsOn() ? (columns + 1) * (rows + 1) : 0; }
  bool operator==(const WarpGrid&) const = default;
};

// How Snapper moves a piece: rig.json.
struct RigPiece final {
  QString name;
  // Empty for a root piece.
  QString parent;
  // The joint, in the piece's own drawing pixels. The piece turns and
  // scales around it, and hangs from its parent there.
  QPointF pivot;
  // Higher draws in front.
  int order = 0;
  // -1 uses the art's default drawing.
  int default_drawing = -1;
  WarpGrid warp;

  bool operator==(const RigPiece&) const = default;
};

// Two pieces bent by dragging a point at the end of the lower one,
// such as upper arm, forearm and the hand's spot.
struct IkChain final {
  QString name;
  QString upper;
  QString lower;
  // The dragged point, in the lower piece's drawing pixels.
  QPointF tip;
  // Which way the middle joint bends.
  bool bends_clockwise = true;

  bool operator==(const IkChain&) const = default;
};

struct Rig final {
  std::vector<RigPiece> pieces;
  std::vector<IkChain> chains;

  bool operator==(const Rig&) const = default;
};

// A doll as the project holds it: Krita's art and Snapper's rig, with
// one rig piece for every art piece of the same name.
struct Doll final {
  QString name;
  // Where the drawings live.
  QString folder;
  DollArt art;
  Rig rig;

  bool operator==(const Doll&) const = default;
};

const ArtPiece* FindArt(const Doll& doll, const QString& name);
const RigPiece* FindRig(const Rig& rig, const QString& name);
RigPiece* FindRig(Rig* rig, const QString& name);
const IkChain* FindChain(const Rig& rig, const QString& name);

// True when parent may become child's parent: it exists (or is empty,
// meaning root) and the rig stays a tree.
bool CanParent(const Rig& rig, const QString& child, const QString& parent);

// The drawing a piece shows for a pose's drawing choice, as a full
// path; empty if the piece has no such drawing.
QString DrawingPath(const Doll& doll, const QString& piece, int drawing);

}  // namespace snapper

#endif  // SNAPPER_MODEL_DOLL_H_
