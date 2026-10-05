#include "ui/keyed_boxes.h"

#include <QSignalBlocker>

#include <cassert>
#include <type_traits>

#include "anim/sampler.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/live_edit.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// The pose a track shows at frame.
PiecePose PoseAt(const Project& project, const TrackRef& track,
                 Frame frame) {
  assert(frame.index() >= 0);
  assert(track.kind != TrackKind::kCamera);
  PiecePose pose;
  const Shot* shot = FindShot(project, track.shot);
  const bool has_shot = shot != nullptr;
  if (has_shot) {
    ReadTrack(*shot, track, [&](const auto& channel) {
      using Held =
          typename std::remove_cvref_t<decltype(channel)>::value_type;
      constexpr bool is_pose = std::is_same_v<Held, PiecePose>;
      if constexpr (is_pose) {
        pose = Sample(channel, frame, PiecePose());
      }
    });
  }
  return pose;
}

}  // namespace

PoseBox::PoseBox(const Managers& managers)
    : QGroupBox(tr("Pose at playhead")), managers_(managers),
      layout_(this), live_(managers.history) {
  assert(managers_.IsComplete());
  SetUpNumber(&fields_[kX], Number::kPixels, tr("x "));
  SetUpNumber(&fields_[kY], Number::kPixels, tr("y "));
  SetUpNumber(&fields_[kScaleX], Number::kScale, tr("x "));
  SetUpNumber(&fields_[kScaleY], Number::kScale, tr("y "));
  SetUpNumber(&fields_[kTurn], Number::kDegrees);
  SetUpNumber(&fields_[kSkew], Number::kLean);
  SetUpNumber(&fields_[kOpacity], Number::kFraction);
  // Named so tests and screen readers find a field by what it is.
  const std::array<const char*, kCount> names = {
      "turn", "x", "y", "scale_x", "scale_y", "lean", "opacity"};
  for (int i = 0; i < kCount; ++i) {
    fields_[static_cast<size_t>(i)].setObjectName(
        QLatin1String(names[static_cast<size_t>(i)]));
  }
  AddPair(&layout_, tr("Move"), &move_row_, &fields_[kX], &fields_[kY]);
  AddPair(&layout_, tr("Scale"), &scale_row_, &fields_[kScaleX],
          &fields_[kScaleY]);
  layout_.addRow(tr("Turn"), &fields_[kTurn]);
  layout_.addRow(tr("Lean"), &fields_[kSkew]);
  layout_.addRow(tr("Opacity"), &fields_[kOpacity]);
  for (QDoubleSpinBox& field : fields_) {
    MakeLive(&field, &live_, tr("Pose"), this, [this] { Commit(); });
  }
  layout_.addRow(tr("Drawing"), &drawing_);
  connect(&drawing_, &QComboBox::activated, this, &PoseBox::SwapDrawing);
  Follow(managers_, this);
  Refresh();
}

void PoseBox::Refresh() {
  const auto spot = SpotOf(managers_);
  const auto track =
      spot ? PickedTrack(managers_, spot->shot) : std::nullopt;
  Explain(this, track ? QString()
                      : tr("Pick a piece or layer on the stage first."));
  if (!track) {
    return;
  }
  const Project& project = managers_.history->current();
  const PiecePose pose = PoseAt(project, *track, spot->local);
  const auto picked = managers_.selection->picks().size();
  setTitle(picked > 1 ? tr("Pose at playhead (%1 picked, changes add to "
                           "each)").arg(picked)
                      : tr("Pose at playhead"));
  ShowNumber(&fields_[kTurn], pose.rotation);
  ShowNumber(&fields_[kX], pose.offset.x());
  ShowNumber(&fields_[kY], pose.offset.y());
  ShowNumber(&fields_[kScaleX], pose.scale_x);
  ShowNumber(&fields_[kScaleY], pose.scale_y);
  ShowNumber(&fields_[kSkew], pose.skew);
  ShowNumber(&fields_[kOpacity], pose.opacity);
  const QSignalBlocker quiet(drawing_);
  drawing_.clear();
  const bool is_piece = track->kind == TrackKind::kPiece;
  const Doll* doll = is_piece ? DollOfLayer(project, track->shot, track->layer)
                              : nullptr;
  const ArtPiece* art = doll != nullptr ? FindArt(*doll, track->piece)
                                        : nullptr;
  drawing_.addItem(tr("Rig default"));
  const int count = art != nullptr ? static_cast<int>(art->drawings.size())
                                   : 0;
  for (int i = 0; i < count; ++i) {
    drawing_.addItem(art->drawings[static_cast<size_t>(i)]);
  }
  drawing_.setCurrentIndex(std::clamp(pose.drawing + 1, 0, count));
  Explain(&drawing_, count > 1 ? QString()
                               : tr("This piece has only one drawing."));
}

void PoseBox::Commit() {
  const auto spot = SpotOf(managers_);
  const auto track = spot ? PickedTrack(managers_, spot->shot) : std::nullopt;
  assert(managers_.pose != nullptr);
  if (!track) {
    return;
  }
  // The fields show the focused pick; a change there is added to every
  // pick, so each keeps its own value plus the same difference.
  const PiecePose shown =
      PoseAt(managers_.history->current(), *track, spot->local);
  PoseDelta delta;
  delta.rotation = fields_[kTurn].value() - shown.rotation;
  delta.offset = QPointF(fields_[kX].value(), fields_[kY].value()) -
                 shown.offset;
  delta.scale_x = fields_[kScaleX].value() - shown.scale_x;
  delta.scale_y = fields_[kScaleY].value() - shown.scale_y;
  delta.skew = fields_[kSkew].value() - shown.skew;
  delta.opacity = fields_[kOpacity].value() - shown.opacity;
  emit Problem(ProblemOf(managers_.pose->Shift(
      managers_.selection->PickedTracks(), spot->local, delta)));
  assert(managers_.history != nullptr);
}

void PoseBox::SwapDrawing(int index) {
  assert(index >= 0);
  const auto spot = SpotOf(managers_);
  const auto track = spot ? PickedTrack(managers_, spot->shot) : std::nullopt;
  if (!track) {
    return;
  }
  emit Problem(ProblemOf(
      managers_.pose->SwapDrawing(*track, spot->local, index - 1)));
  assert(managers_.pose != nullptr);
}

CameraBox::CameraBox(const Managers& managers)
    : QGroupBox(tr("Camera at playhead")), managers_(managers),
      layout_(this), live_(managers.history) {
  assert(managers_.IsComplete());
  SetUpNumber(&fields_[kX], Number::kPixels, tr("x "));
  SetUpNumber(&fields_[kY], Number::kPixels, tr("y "));
  SetUpNumber(&fields_[kZoom], Number::kZoom);
  SetUpNumber(&fields_[kTurn], Number::kDegrees);
  SetUpNumber(&fields_[kShake], Number::kSize);
  const std::array<const char*, kCount> names = {"x", "y", "zoom", "turn",
                                                 "shake"};
  for (int i = 0; i < kCount; ++i) {
    fields_[static_cast<size_t>(i)].setObjectName(
        QLatin1String(names[static_cast<size_t>(i)]));
  }
  AddPair(&layout_, tr("Look at"), &look_row_, &fields_[kX], &fields_[kY]);
  layout_.addRow(tr("Zoom"), &fields_[kZoom]);
  layout_.addRow(tr("Turn"), &fields_[kTurn]);
  layout_.addRow(tr("Shake"), &fields_[kShake]);
  for (QDoubleSpinBox& field : fields_) {
    MakeLive(&field, &live_, tr("Move camera"), this, [this] { Commit(); });
  }
  Follow(managers_, this);
  Refresh();
}

void CameraBox::Refresh() {
  const auto spot = SpotOf(managers_);
  Explain(this, spot ? QString() : tr("Move the playhead onto a shot."));
  if (!spot) {
    return;
  }
  const Shot* shot = FindShot(managers_.history->current(), spot->shot);
  assert(shot != nullptr);
  const CameraPose camera = Sample(shot->camera, spot->local, CameraPose());
  ShowNumber(&fields_[kX], camera.center.x());
  ShowNumber(&fields_[kY], camera.center.y());
  ShowNumber(&fields_[kZoom], camera.zoom);
  ShowNumber(&fields_[kTurn], camera.rotation);
  ShowNumber(&fields_[kShake], camera.shake);
}

void CameraBox::Commit() {
  const auto spot = SpotOf(managers_);
  assert(managers_.pose != nullptr);
  if (!spot) {
    return;
  }
  const CameraPose camera{
      QPointF(fields_[kX].value(), fields_[kY].value()),
      fields_[kZoom].value(), fields_[kTurn].value(), fields_[kShake].value()};
  emit Problem(
      ProblemOf(managers_.pose->SetCamera(spot->shot, spot->local, camera)));
  assert(managers_.history != nullptr);
}

}  // namespace snapper
