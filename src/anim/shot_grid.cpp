#include "anim/shot_grid.h"

#include <cassert>
#include <cmath>

#include "anim/warp.h"

namespace snapper {

QPointF ShiftOf(const LinkShifts& shifts, const QString& piece, int frame) {
  assert(frame >= 0);
  assert(!piece.isEmpty());
  const auto found = shifts.find(piece);
  const bool is_shifted = found != shifts.end() &&
                          static_cast<size_t>(frame) < found->second.size();
  return is_shifted ? found->second[static_cast<size_t>(frame)] : QPointF();
}

std::optional<ShotGrid> ShotGridOf(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const QString& piece,
    Frame frame, QPointF shift) {
  assert(frame.index() >= 0);
  assert(!piece.isEmpty());
  const auto transform = placed.find(piece);
  const ArtPiece* art = FindArt(doll, piece);
  const RigPiece* rig = FindRig(doll.rig, piece);
  const bool is_placed =
      transform != placed.end() && art != nullptr && rig != nullptr;
  if (!is_placed) {
    return std::nullopt;
  }
  const auto pose = poses.find(piece);
  ShotGrid grid;
  grid.points = WarpedPoints(
      art->size, rig->warp,
      pose != poses.end() ? pose->second.warp : std::vector<QPointF>());
  grid.to_shot = transform->second * LayerTransform(layer, frame);
  for (QPointF& point : grid.points) {
    point = grid.to_shot.map(point) + shift;
  }
  grid.to_shot *= QTransform::fromTranslate(shift.x(), shift.y());
  return grid;
}

QPointF DrawnDown(const QTransform& to_shot) {
  assert(to_shot.isAffine());
  assert(std::isfinite(to_shot.m22()));
  const QPointF down = to_shot.map(QPointF(0.0, 1.0)) - to_shot.map(QPointF());
  const double length = std::hypot(down.x(), down.y());
  const bool is_flat = !(length > 1e-9);
  return is_flat ? QPointF(0.0, 1.0) : down / length;
}

double CellLength(WarpGrid grid, QSize size, const QTransform& to_shot) {
  assert(grid.IsOn());
  assert(to_shot.isAffine());
  const double across = size.width() / static_cast<double>(grid.columns);
  const double down = size.height() / static_cast<double>(grid.rows);
  return (across + down) / 2.0 * std::sqrt(std::abs(to_shot.determinant()));
}

}  // namespace snapper
