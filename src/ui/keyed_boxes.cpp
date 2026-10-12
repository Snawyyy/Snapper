#include "ui/keyed_boxes.h"

#include <QSignalBlocker>

#include <array>
#include <cassert>
#include <type_traits>
#include <vector>

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
#include "ui/pose_controls.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// The Move pad's side, and the height both top cells share.
constexpr int kPadSide = 96;
// The Turn dial: big, as the control used most after Move.
constexpr int kTurnDial = 72;
// The top row's width shares: Move gets the golden ratio's long part,
// Turn the short one (62 to 38).
constexpr int kGoldenLong = 62;
constexpr int kGoldenShort = 38;
// Space between the panel's rows, on an 8 px rhythm.
constexpr int kRowGap = 8;
// Scale faders span 0 to this; past it the cap rests at the end.
constexpr double kScaleShown = 3.0;
// The camera's zoom fader spans up to this; the shake fader up to
// kShakeShown pixels.
constexpr double kZoomShown = 4.0;
constexpr double kShakeShown = 60.0;
// Skew's whole range, both ways.
constexpr double kSkewShown = 85.0;

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

void PoseBox::ShowPose(const Project& project,
                       const std::vector<TrackRef>& tracks, Frame frame,
                       const PiecePose& focus) {
  assert(frame.index() >= 0);
  assert(!tracks.empty());
  // What each pick shows, field by field: several that differ say Mixed.
  std::array<std::vector<double>, kCount> spread;
  for (const TrackRef& track : tracks) {
    const PiecePose p = PoseAt(project, track, frame);
    const std::array<double, kCount> shown = {
        p.rotation, p.offset.x(), p.offset.y(), p.scale_x, p.scale_y,
        p.skew,     p.opacity,    p.lean,       p.swivel};
    for (size_t i = 0; i < shown.size(); ++i) {
      spread[i].push_back(shown[i]);
    }
  }
  ShowNumber(&fields_[kTurn], focus.rotation, spread[kTurn]);
  ShowNumber(&fields_[kX], focus.offset.x(), spread[kX]);
  ShowNumber(&fields_[kY], focus.offset.y(), spread[kY]);
  ShowNumber(&fields_[kScaleX], focus.scale_x, spread[kScaleX]);
  ShowNumber(&fields_[kScaleY], focus.scale_y, spread[kScaleY]);
  ShowNumber(&fields_[kSkew], focus.skew, spread[kSkew]);
  ShowNumber(&fields_[kOpacity], focus.opacity, spread[kOpacity]);
  ShowNumber(&fields_[kLean], focus.lean, spread[kLean]);
  ShowNumber(&fields_[kSwivel], focus.swivel, spread[kSwivel]);
}

PoseBox::PoseBox(const Managers& managers)
    : QGroupBox(tr("Pose at playhead")), managers_(managers),
      layout_(this), live_(managers.history) {
  assert(managers_.IsComplete());
  SetUpNumber(&fields_[kX], Number::kPixels);
  SetUpNumber(&fields_[kY], Number::kPixels);
  SetUpNumber(&fields_[kScaleX], Number::kScale);
  SetUpNumber(&fields_[kScaleY], Number::kScale);
  SetUpNumber(&fields_[kTurn], Number::kDegrees);
  SetUpNumber(&fields_[kSkew], Number::kLean);
  SetUpNumber(&fields_[kOpacity], Number::kFraction);
  SetUpNumber(&fields_[kLean], Number::kDegrees);
  SetUpNumber(&fields_[kSwivel], Number::kDegrees);
  // Named so tests and screen readers find a field by what it is.
  const std::array<const char*, kCount> names = {
      "turn", "x",       "y",    "scale_x", "scale_y",
      "skew", "opacity", "lean", "swivel"};
  for (int i = 0; i < kCount; ++i) {
    fields_[static_cast<size_t>(i)].setObjectName(
        QLatin1String(names[static_cast<size_t>(i)]));
  }
  // The two most used controls share the top row at one height, then
  // the sliders, then the rarely used turns; the fields stay as small
  // readouts you can still type in.
  layout_.setVerticalSpacing(kRowGap);
  layout_.addRow(MakeCellRow(
      {MakePadCell(tr("Move"), &fields_[kX], &fields_[kY], &live_, this),
       MakeDialCell(tr("Turn"), &fields_[kTurn], &live_, kTurnDial,
                    kPadSide, this)},
      {kGoldenLong, kGoldenShort}, this));
  layout_.addRow(MakeFaderRow(tr("Scale x"), &fields_[kScaleX], &live_,
                              kScaleShown, 1.0, this));
  layout_.addRow(MakeFaderRow(tr("Scale y"), &fields_[kScaleY], &live_,
                              kScaleShown, 1.0, this));
  layout_.addRow(MakeFaderRow(tr("Skew"), &fields_[kSkew], &live_,
                              kSkewShown, 0.0, this));
  layout_.addRow(MakeFaderRow(tr("Opacity"), &fields_[kOpacity], &live_, 1.0,
                              1.0, this));
  turns_row_ = MakeCellRow(
      {MakeMiniDial(tr("Lean"), &fields_[kLean], &live_, this),
       MakeMiniDial(tr("Swivel"), &fields_[kSwivel], &live_, this)},
      {}, this);
  layout_.addRow(turns_row_);
  for (QDoubleSpinBox& field : fields_) {
    MakeLive(&field, &live_, tr("Pose"), this, [this] { Commit(); });
  }
  layout_.addRow(tr("Drawing"), &drawing_);
  assert(layout_.rowCount() >= 6);
  connect(&drawing_, &QComboBox::activated, this, &PoseBox::SwapDrawing);
  Follow(managers_, this);
  Refresh();
}

void PoseBox::Refresh() {
  assert(managers_.history != nullptr);
  const auto spot = SpotOf(managers_);
  const auto track =
      spot ? PickedTrack(managers_, spot->shot) : std::nullopt;
  Explain(this, track ? QString()
                      : tr("Pick a piece or layer on the stage first."));
  if (!track) {
    return;
  }
  assert(spot.has_value());
  const Project& project = managers_.history->current();
  const PiecePose pose = PoseAt(project, *track, spot->local);
  const auto picked = managers_.selection->picks().size();
  setTitle(picked > 1 ? tr("Pose at playhead (%1 picked, changes add to "
                           "each)").arg(picked)
                      : tr("Pose at playhead"));
  ShowPose(project, managers_.selection->PickedTracks(), spot->local, pose);
  const bool is_whole_doll =
      track->kind == TrackKind::kLayer &&
      DollOfLayer(project, track->shot, track->layer) != nullptr;
  // Lean and Swivel turn a whole doll; on a limb they have no meaning.
  const bool is_turns_new = turns_row_->isHidden() == is_whole_doll;
  if (is_turns_new) {
    layout_.setRowVisible(turns_row_, is_whole_doll);
  }
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
  // Only a piece with a choice of drawings shows the picker.
  const bool has_choice = count > 1;
  const bool is_drawing_new = drawing_.isHidden() == has_choice;
  if (is_drawing_new) {
    layout_.setRowVisible(&drawing_, has_choice);
  }
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
  delta.lean = fields_[kLean].value() - shown.lean;
  delta.swivel = fields_[kSwivel].value() - shown.swivel;
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
  SetUpNumber(&fields_[kX], Number::kPixels);
  SetUpNumber(&fields_[kY], Number::kPixels);
  SetUpNumber(&fields_[kZoom], Number::kZoom);
  SetUpNumber(&fields_[kTurn], Number::kDegrees);
  SetUpNumber(&fields_[kShake], Number::kSize);
  const std::array<const char*, kCount> names = {"x", "y", "zoom", "turn",
                                                 "shake"};
  for (int i = 0; i < kCount; ++i) {
    fields_[static_cast<size_t>(i)].setObjectName(
        QLatin1String(names[static_cast<size_t>(i)]));
  }
  layout_.setVerticalSpacing(kRowGap);
  layout_.addRow(MakeCellRow(
      {MakePadCell(tr("Look at"), &fields_[kX], &fields_[kY], &live_, this),
       MakeDialCell(tr("Turn"), &fields_[kTurn], &live_, kTurnDial,
                    kPadSide, this)},
      {kGoldenLong, kGoldenShort}, this));
  layout_.addRow(MakeFaderRow(tr("Zoom"), &fields_[kZoom], &live_,
                              kZoomShown, 1.0, this));
  layout_.addRow(MakeFaderRow(tr("Shake"), &fields_[kShake], &live_,
                              kShakeShown, 0.0, this));
  for (QDoubleSpinBox& field : fields_) {
    MakeLive(&field, &live_, tr("Move camera"), this, [this] { Commit(); });
  }
  assert(layout_.rowCount() >= 3);
  Follow(managers_, this);
  Refresh();
}

void CameraBox::Refresh() {
  assert(managers_.history != nullptr);
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
