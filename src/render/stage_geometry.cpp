#include "render/stage_geometry.h"

#include <QRectF>

#include <cassert>
#include <variant>

#include "anim/doll_lean.h"
#include "anim/doll_pose.h"
#include "anim/links.h"
#include "anim/warp.h"
#include "render/frame_renderer.h"
#include "render/layer_painter.h"

namespace snapper {
namespace {

struct PosedDoll final {
  const Doll* doll = nullptr;
  // The poses shown: keys, drags, physics and collider pushes.
  PoseMap poses;
  std::map<QString, QTransform> pieces;
  QTransform to_screen;
};

// The doll layer's pieces in layer space, and layer space to screen.
std::optional<PosedDoll> Pose(const Project& project, const Shot& shot,
                              LayerId layer, Frame local, double scale,
                              PoseMemo* memo) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  const auto* posed =
      found != nullptr ? std::get_if<DollLayer>(&found->content) : nullptr;
  const Doll* doll =
      posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
  const bool is_doll = doll != nullptr;
  if (!is_doll) {
    return std::nullopt;
  }
  LinkSolver links(project, shot, memo);
  PoseMap poses = links.Poses(*doll, *found, local);
  auto pieces = PieceTransforms(*doll, poses);
  return PosedDoll{doll, std::move(poses), std::move(pieces),
                   links.LayerTransform(*found, local) *
                       FrameRenderer::ViewTransform(project, shot, local,
                                                    scale)};
}

// piece's grid points in its drawing pixels, pushed by its warp in
// poses (at rest when the warp does not fit the grid).
std::vector<QPointF> WarpOf(const PoseMap& poses, const RigPiece& rig,
                            QSize size, const QString& piece) {
  assert(rig.warp.IsOn());
  assert(!piece.isEmpty());
  const auto pose = poses.find(piece);
  const bool is_fitting =
      pose != poses.end() &&
      static_cast<int>(pose->second.warp.size()) == rig.warp.PointCount();
  return WarpedPoints(size, rig.warp,
                      is_fitting ? pose->second.warp
                                 : std::vector<QPointF>());
}

}  // namespace

std::optional<QTransform> LayerToScreen(const Project& project,
                                        const Shot& shot, LayerId layer,
                                        Frame local, double scale,
                                        PoseMemo* memo) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  const bool is_found = found != nullptr;
  if (!is_found) {
    return std::nullopt;
  }
  LinkSolver links(project, shot, memo);
  return links.LayerTransform(*found, local) *
         FrameRenderer::ViewTransform(project, shot, local, scale);
}

QTransform LayerParentToScreen(const Project& project, const Shot& shot,
                               const Layer& layer, Frame local, double scale,
                               PoseMemo* memo) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const QTransform view =
      FrameRenderer::ViewTransform(project, shot, local, scale);
  const bool is_never_held = shot.holds.empty();
  if (is_never_held) {
    return view;
  }
  LinkSolver links(project, shot, memo);
  return links.HeldMove(layer, local) * view;
}

std::optional<QTransform> PieceParentToScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale,
                                              PoseMemo* memo) {
  assert(!piece.isEmpty());
  assert(scale > 0.0);
  const auto posed = Pose(project, shot, layer, local, scale, memo);
  const RigPiece* rig =
      posed ? FindRig(posed->doll->rig, piece) : nullptr;
  const bool is_piece = rig != nullptr;
  if (!is_piece) {
    return std::nullopt;
  }
  const auto parent = posed->pieces.find(rig->parent);
  const bool has_parent =
      !rig->parent.isEmpty() && parent != posed->pieces.end();
  // A piece's offset is measured in its parent's frame (the layer's for
  // a root piece); only that frame's turn and size matter for moving.
  const QTransform frame = has_parent ? parent->second : QTransform();
  return frame * posed->to_screen;
}

std::optional<PieceOutline> PieceOnScreen(const Project& project,
                                          const Shot& shot, LayerId layer,
                                          const QString& piece, Frame local,
                                          double scale,
                                          PoseMemo* memo) {
  assert(!piece.isEmpty());
  assert(scale > 0.0);
  const auto posed = Pose(project, shot, layer, local, scale, memo);
  const ArtPiece* art = posed ? FindArt(*posed->doll, piece) : nullptr;
  const RigPiece* rig = posed ? FindRig(posed->doll->rig, piece) : nullptr;
  const bool is_piece = art != nullptr && rig != nullptr &&
                        posed->pieces.contains(piece);
  if (!is_piece) {
    return std::nullopt;
  }
  const QTransform to_screen = posed->pieces.at(piece) * posed->to_screen;
  return PieceOutline{
      to_screen.map(QPolygonF(QRectF(QPointF(), QSizeF(art->size)))),
      to_screen.map(rig->pivot)};
}

std::optional<WarpOnScreen> PieceWarpOnScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale,
                                              PoseMemo* memo) {
  assert(!piece.isEmpty());
  assert(scale > 0.0);
  const auto posed = Pose(project, shot, layer, local, scale, memo);
  const ArtPiece* art = posed ? FindArt(*posed->doll, piece) : nullptr;
  const RigPiece* rig = posed ? FindRig(posed->doll->rig, piece) : nullptr;
  const bool has_grid = art != nullptr && rig != nullptr &&
                        rig->warp.IsOn() && posed->pieces.contains(piece);
  if (!has_grid) {
    return std::nullopt;
  }
  const auto& keys = std::get<DollLayer>(FindLayer(shot, layer)->content);
  const std::vector<QPointF> keyed =
      WarpOf(SamplePoses(*posed->doll, keys, local), *rig, art->size, piece);
  const std::vector<QPointF> shown =
      WarpOf(posed->poses, *rig, art->size, piece);
  // The dots as this layer has them (its own, or the doll's default).
  const PieceDots dots = DotsOf(*rig, layer);
  const bool has_reach =
      static_cast<int>(dots.warp_reach.size()) == rig->warp.PointCount();
  std::vector<std::optional<NodeKind>> kinds(shown.size());
  for (const DragNode& node : dots.drag_nodes) {
    const bool is_inside =
        node.point >= 0 && node.point < static_cast<int>(kinds.size());
    if (is_inside) {
      kinds[static_cast<size_t>(node.point)] = node.kind;
    }
  }
  std::vector<int> moves;
  for (const PointMotion& motion : dots.point_motions) {
    moves.push_back(motion.point);
  }
  return WarpOnScreen{
      rig->warp, shown, keyed,
      has_reach ? dots.warp_reach
                : std::vector<double>(
                      static_cast<size_t>(rig->warp.PointCount()), 0.0),
      kinds, moves, art->size, posed->pieces.at(piece) * posed->to_screen};
}

std::vector<IkHandle> IkHandles(const Project& project, const Shot& shot,
                                LayerId layer, Frame local, double scale,
                                PoseMemo* memo) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  std::vector<IkHandle> handles;
  const auto posed = Pose(project, shot, layer, local, scale, memo);
  if (!posed) {
    return handles;
  }
  for (const IkChain& chain : posed->doll->rig.chains) {
    const auto lower = posed->pieces.find(chain.lower);
    const bool is_placed = lower != posed->pieces.end();
    if (is_placed) {
      handles.push_back(
          {chain.name, (lower->second * posed->to_screen).map(chain.tip)});
    }
  }
  return handles;
}

}  // namespace snapper
