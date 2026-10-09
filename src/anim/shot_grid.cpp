#include "anim/shot_grid.h"

#include <cassert>

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

}  // namespace snapper
