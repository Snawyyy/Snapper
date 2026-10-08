#include "render/stage_geometry.h"

#include <QRectF>

#include <cassert>
#include <variant>

#include "anim/doll_lean.h"
#include "anim/doll_pose.h"
#include "anim/warp.h"
#include "render/frame_renderer.h"
#include "render/layer_painter.h"

namespace snapper {
namespace {

struct PosedDoll final {
  const Doll* doll = nullptr;
  std::map<QString, QTransform> pieces;
  QTransform to_screen;
};

// The doll layer's pieces in layer space, and layer space to screen.
std::optional<PosedDoll> Pose(const Project& project, const Shot& shot,
                              LayerId layer, Frame local, double scale) {
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
  return PosedDoll{doll,
                   PieceTransforms(*doll, ShownPoses(*doll, *found, local)),
                   LayerTransform(*found, local) *
                       FrameRenderer::ViewTransform(project, shot, local,
                                                    scale)};
}

}  // namespace

std::optional<QTransform> LayerToScreen(const Project& project,
                                        const Shot& shot, LayerId layer,
                                        Frame local, double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  const bool is_found = found != nullptr;
  if (!is_found) {
    return std::nullopt;
  }
  return LayerTransform(*found, local) *
         FrameRenderer::ViewTransform(project, shot, local, scale);
}

std::optional<QTransform> PieceParentToScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale) {
  assert(!piece.isEmpty());
  assert(scale > 0.0);
  const auto posed = Pose(project, shot, layer, local, scale);
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
                                          double scale) {
  assert(!piece.isEmpty());
  assert(scale > 0.0);
  const auto posed = Pose(project, shot, layer, local, scale);
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
                                              Frame local, double scale) {
  assert(!piece.isEmpty());
  assert(scale > 0.0);
  const auto posed = Pose(project, shot, layer, local, scale);
  const ArtPiece* art = posed ? FindArt(*posed->doll, piece) : nullptr;
  const RigPiece* rig = posed ? FindRig(posed->doll->rig, piece) : nullptr;
  const bool has_grid = art != nullptr && rig != nullptr &&
                        rig->warp.IsOn() && posed->pieces.contains(piece);
  if (!has_grid) {
    return std::nullopt;
  }
  const auto& keys = std::get<DollLayer>(FindLayer(shot, layer)->content);
  const PoseMap poses = SamplePoses(keys, local);
  const auto pose = poses.find(piece);
  const bool is_fitting =
      pose != poses.end() &&
      static_cast<int>(pose->second.warp.size()) == rig->warp.PointCount();
  const std::vector<QPointF> offsets =
      is_fitting ? pose->second.warp : std::vector<QPointF>();
  const bool has_reach =
      static_cast<int>(rig->warp_reach.size()) == rig->warp.PointCount();
  std::vector<int> drags;
  for (const DragNode& node : rig->drag_nodes) {
    drags.push_back(node.point);
  }
  std::vector<int> moves;
  for (const PointMotion& motion : rig->point_motions) {
    moves.push_back(motion.point);
  }
  return WarpOnScreen{
      rig->warp, WarpedPoints(art->size, rig->warp, offsets),
      has_reach ? rig->warp_reach
                : std::vector<double>(
                      static_cast<size_t>(rig->warp.PointCount()), 0.0),
      drags, moves, art->size, posed->pieces.at(piece) * posed->to_screen};
}

std::vector<IkHandle> IkHandles(const Project& project, const Shot& shot,
                                LayerId layer, Frame local, double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  std::vector<IkHandle> handles;
  const auto posed = Pose(project, shot, layer, local, scale);
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
