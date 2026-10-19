#include "ui/keyed_boxes.h"

#include <QSignalBlocker>

#include <array>
#include <cassert>
#include <set>
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
#include "ui/panel_kit.h"
#include "ui/playhead.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"
#include "ui/reset_on_double_click.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The Move pad's side, and the height both top cells share.
constexpr int kPadSide = 96;
// The Turn dial: big, as the control used most after Move.
constexpr int kTurnDial = 72;
// Scale faders span 0 to this; past it the cap rests at the end.
constexpr double kScaleShown = 3.0;
// The camera's zoom fader spans up to this; the shake fader up to
// kShakeShown pixels.
constexpr double kZoomShown = 4.0;
constexpr double kShakeShown = 60.0;
// Skew's whole range, both ways.
constexpr double kSkewShown = 85.0;

QString OrderHelp() {
  return QObject::tr(
      "How many places in front (above 0) or behind (below 0) of its "
      "usual place this is drawn. Keyed at the playhead and held until "
      "the next key. Ctrl+] and Ctrl+[ step it; double click resets.");
}

// The pose a track shows at frame.
LayerPose PoseAt(const Project& project, const TrackRef& track,
                 Frame frame) {
  assert(frame.index() >= 0);
  assert(track.kind != TrackKind::kCamera);
  LayerPose pose;
  const Shot* shot = FindShot(project, track.shot);
  const bool has_shot = shot != nullptr;
  if (has_shot) {
    ReadTrack(*shot, track, [&](const auto& channel) {
      using Held =
          typename std::remove_cvref_t<decltype(channel)>::value_type;
      constexpr bool is_pose = std::is_same_v<Held, LayerPose>;
      if constexpr (is_pose) {
        pose = Sample(channel, frame, LayerPose());
      }
    });
  }
  return pose;
}

}  // namespace

void PoseBox::ShowPose(const Project& project,
                       const std::vector<TrackRef>& tracks, Frame frame,
                       const LayerPose& focus) {
  assert(frame.index() >= 0);
  assert(!tracks.empty());
  // What each pick shows, field by field: several that differ say Mixed.
  std::array<std::vector<double>, kCount> spread;
  for (const TrackRef& track : tracks) {
    const LayerPose p = PoseAt(project, track, frame);
    const std::array<double, kCount> shown = {
        p.rotation, p.offset.x(), p.offset.y(), p.scale_x, p.scale_y,
        p.skew,     p.opacity,    static_cast<double>(p.order)};
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
  ShowNumber(&fields_[kOrder], focus.order, spread[kOrder]);
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
  SetUpNumber(&fields_[kOrder], Number::kPlaces);
  // Named so tests and screen readers find a field by what it is.
  const std::array<const char*, kCount> names = {
      "turn", "x", "y", "scale_x", "scale_y", "skew", "opacity", "order"};
  for (int i = 0; i < kCount; ++i) {
    fields_[static_cast<size_t>(i)].setObjectName(
        QLatin1String(names[static_cast<size_t>(i)]));
  }
  // The two most used controls share the top row at one height, then
  // the sliders, then the draw order; the fields stay as small
  // readouts you can still type in.
  // Move takes the golden ratio's long part of the top row, Turn the
  // short one.
  layout_.setVerticalSpacing(theme::kRowGap);
  layout_.addRow(GoldenRow(
      MakePadCell(tr("Move"), &fields_[kX], &fields_[kY], &live_, this),
      MakeDialCell(tr("Turn"), &fields_[kTurn], &live_, kTurnDial, kPadSide,
                   this),
      this));
  layout_.addRow(MakeFaderRow(tr("Scale x"), &fields_[kScaleX], &live_,
                              kScaleShown, 1.0, this));
  layout_.addRow(MakeFaderRow(tr("Scale y"), &fields_[kScaleY], &live_,
                              kScaleShown, 1.0, this));
  layout_.addRow(MakeFaderRow(tr("Skew"), &fields_[kSkew], &live_,
                              kSkewShown, 0.0, this));
  layout_.addRow(MakeFaderRow(tr("Opacity"), &fields_[kOpacity], &live_, 1.0,
                              1.0, this));
  layout_.addRow(MakeCellRow(
      {MakeSpinCell(tr("Order"), &fields_[kOrder], &live_, this)}, {}, this));
  SetUpOrder();
  for (int i = 0; i < kOrder; ++i) {
    MakeLive(&fields_[static_cast<size_t>(i)], &live_, tr("Pose"), this,
             [this] { Commit(); });
  }
  assert(layout_.rowCount() >= 6);
  Follow(managers_, this);
  Refresh();
}

void PoseBox::SetUpOrder() {
  QDoubleSpinBox& order = fields_[kOrder];
  order.setToolTip(OrderHelp());
  ResetOnDoubleClick::Install(&order, 0.0, &live_);
  MakeLiveNamed(&order, &live_, this,
                [this] {
                  return managers_.pose->OrderName(
                      managers_.selection->PickedTracks());
                },
                [this] { Commit(); });
}

void PoseBox::Refresh() {
  assert(managers_.history != nullptr);
  const auto spot = SpotOf(managers_);
  const auto track =
      spot ? PickedTrack(managers_, spot->shot) : std::nullopt;
  const QString why_not =
      track ? QString() : tr("Pick a layer on the stage first.");
  Explain(this, why_not);
  Explain(&fields_[kOrder], why_not, OrderHelp());
  if (!track) {
    return;
  }
  assert(spot.has_value());
  const Project& project = managers_.history->current();
  const LayerPose pose = PoseAt(project, *track, spot->local);
  const auto picked = managers_.selection->picks().size();
  setTitle(picked > 1 ? tr("Pose at playhead (%1 picked, changes add to "
                           "each)").arg(picked)
                      : tr("Pose at playhead"));
  ShowPose(project, managers_.selection->PickedTracks(), spot->local, pose);
}

void PoseBox::Commit() {
  const auto spot = SpotOf(managers_);
  const auto track = spot ? PickedTrack(managers_, spot->shot) : std::nullopt;
  assert(managers_.pose != nullptr);
  if (!track) {
    return;
  }
  // With keys picked, every picked frame changes. A number typed in a
  // field sets every pick and frame to it; a dial, pad or fader drag
  // adds the same difference to each, so each keeps its own value.
  const std::set<Frame> picked =
      managers_.selection->EditFrames(spot->local);
  const std::vector<Frame> frames(picked.begin(), picked.end());
  const LayerPose shown =
      PoseAt(managers_.history->current(), *track, spot->local);
  const std::array<double, kCount> was = {
      shown.rotation, shown.offset.x(), shown.offset.y(),
      shown.scale_x,  shown.scale_y,    shown.skew,
      shown.opacity,  static_cast<double>(shown.order)};
  std::array<double, kCount> change{};
  for (size_t i = 0; i < change.size(); ++i) {
    change[i] = fields_[i].value() - was[i];
    const bool is_typed = fields_[i].hasFocus() && change[i] != 0.0;
    if (is_typed) {
      emit Problem(ProblemOf(managers_.pose->SetField(
          managers_.selection->PickedTracks(), frames,
          static_cast<PoseField>(i), fields_[i].value())));
      return;
    }
  }
  PoseDelta delta;
  delta.rotation = change[kTurn];
  delta.offset = QPointF(change[kX], change[kY]);
  delta.scale_x = change[kScaleX];
  delta.scale_y = change[kScaleY];
  delta.skew = change[kSkew];
  delta.opacity = change[kOpacity];
  delta.order = change[kOrder];
  emit Problem(ProblemOf(managers_.pose->Shift(
      managers_.selection->PickedTracks(), frames, delta)));
  assert(managers_.history != nullptr);
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
  layout_.setVerticalSpacing(theme::kRowGap);
  layout_.addRow(GoldenRow(
      MakePadCell(tr("Look at"), &fields_[kX], &fields_[kY], &live_, this),
      MakeDialCell(tr("Turn"), &fields_[kTurn], &live_, kTurnDial, kPadSide,
                   this),
      this));
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
