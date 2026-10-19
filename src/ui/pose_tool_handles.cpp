// PoseTool's grabbing of handles: a picked character's bones.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <variant>
#include <vector>

#include "edit/character_pose_edits.h"
#include "edit/character_pose_manager.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/pose_tool.h"

namespace snapper {

QString PoseTool::BoneKey(LayerId layer, BoneId bone) {
  assert(layer.IsValid());
  assert(bone.IsValid());
  return QStringLiteral("bone/%1/%2").arg(layer.value()).arg(bone.value());
}

void PoseTool::Observe() {
  assert(managers_.selection != nullptr);
  assert(clock_.isValid());
  const bool is_shown = !managers_.selection->picks().empty();
  grace_.Update(is_shown, NowMs());
  if (is_shown) {
    shown_picks_ = managers_.selection->picks();
    shown_layer_ = managers_.selection->layer();
    shown_shot_ = managers_.selection->shot();
  }
}

std::set<Pick> PoseTool::GrabbablePicks(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.selection != nullptr);
  const std::set<Pick>& live = managers_.selection->picks();
  const bool is_live =
      !live.empty() && managers_.selection->shot() == frame.shot;
  if (is_live) {
    return live;
  }
  const bool is_graced =
      shown_shot_ == frame.shot && grace_.IsGrabbable(false, NowMs());
  return is_graced ? shown_picks_ : std::set<Pick>();
}

std::vector<PoseTool::Handle> PoseTool::HandlesOf(
    const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  std::vector<Handle> handles;
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const std::set<Pick> picks = GrabbablePicks(frame);
  const bool has_handles = shot != nullptr && !picks.empty();
  if (!has_handles) {
    return handles;
  }
  const bool is_live = !managers_.selection->picks().empty() &&
                       managers_.selection->shot() == frame.shot;
  const LayerId layer =
      is_live ? managers_.selection->layer() : shown_layer_;
  for (const PosedBone& bone :
       layer.IsValid()
           ? CharacterBonesOnScreen(project, *shot, layer, frame.local,
                                    frame.scale)
           : std::vector<PosedBone>()) {
    handles.push_back({GrabKind::kBone, BoneKey(layer, bone.id),
                       bone.tip + frame.corner, layer, bone.id});
  }
  return handles;
}

std::optional<PoseTool::Handle> PoseTool::HandleAt(
    QPointF point, const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const std::vector<Handle> handles = HandlesOf(frame);
  std::vector<QPointF> spots;
  for (const Handle& handle : handles) {
    spots.push_back(handle.spot);
  }
  const int nearest = NearestGrab(spots, point);
  const bool is_hit = nearest >= 0;
  return is_hit ? std::optional<Handle>(handles[static_cast<size_t>(nearest)])
                : std::nullopt;
}

PoseTool::Hover PoseTool::HoverAt(QPointF point,
                                  const StageFrame& frame) const {
  assert(cache_ != nullptr);
  assert(frame.scale > 0.0);
  Hover hover;
  const auto handle = HandleAt(point, frame);
  if (handle) {
    hover.kind = handle->kind;
    hover.key = handle->key;
    return hover;
  }
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto hit =
      shot != nullptr ? HitTest(project, *shot, frame.local,
                                point - frame.corner, frame.scale, cache_)
                      : std::nullopt;
  if (hit) {
    const auto& picks = managers_.selection->picks();
    const Pick pick{hit->layer};
    const bool is_unpicked = !picks.contains(pick);
    hover.pick = is_unpicked ? std::optional<Pick>(pick) : std::nullopt;
  }
  return hover;
}

bool PoseTool::PressHandle(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const auto handle = HandleAt(point, frame);
  if (!handle) {
    return false;
  }
  GrabBone(*handle, point, frame);
  return drag_ != nullptr;
}

void PoseTool::GrabBone(const Handle& handle, QPointF point,
                        const StageFrame& frame) {
  assert(handle.kind == GrabKind::kBone);
  assert(frame.scale > 0.0);
  const Character* character =
      managers_.character_poses != nullptr
          ? managers_.character_poses->CharacterOf(frame.shot, handle.layer)
          : nullptr;
  const Bone* bone =
      character != nullptr
          ? FindBone(*character, handle.bone)
          : nullptr;
  if (bone == nullptr) {
    return;
  }
  auto drag = std::make_unique<Drag>();
  drag->kind = Kind::kBone;
  drag->frame = frame;
  drag->layer = handle.layer;
  drag->bone = handle.bone;
  drag->press = point;
  drag->start = point;
  drag->scope = std::make_unique<EditScope>(
      managers_.history, QObject::tr("Bend %1").arg(bone->name));
  drag_ = std::move(drag);
}

}  // namespace snapper
