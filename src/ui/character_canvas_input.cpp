#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/bone_ik.h"
#include "anim/stroke_fit.h"
#include "edit/character_manager.h"
#include "edit/character_pose_edits.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/vector_edits.h"
#include "render/character_paint.h"
#include "ui/character_canvas.h"
#include "ui/pick_keys.h"

namespace snapper {
namespace {

// A press that moves less than this is a click.
constexpr double kClickSlop = 4.0;
// How far, in widget pixels, a freehand line may stray from its points.
constexpr double kStrokeTolerance = 2.5;
// A freehand stroke ending this close to its start closes.
constexpr double kCloseReach = 14.0;

QString ProblemOf(const Error& error) { return error.message; }

}  // namespace

void CharacterCanvas::Report(const QString& problem) {
  assert(problem.size() < 100000);
  assert(session_ != nullptr);
  emit Problem(problem);
}

void CharacterCanvas::OpenScope(const QString& label) {
  assert(!label.isEmpty());
  assert(history_ != nullptr);
  scope_ = std::make_unique<EditScope>(history_, label);
}

void CharacterCanvas::CloseScope(bool keep) {
  assert(history_ != nullptr);
  assert(drag_ != Drag::kNone || scope_ == nullptr);
  if (scope_ != nullptr && !keep) {
    scope_->Cancel();
  }
  scope_.reset();
}

void CharacterCanvas::PoseStop(const CharacterPose& pose) {
  assert(IsPosingStop());
  assert(characters_ != nullptr);
  const auto done = characters_->SetActionStop(
      session_->character(), session_->action_bone(), session_->action_stop(),
      pose);
  if (!done) {
    Report(ProblemOf(done.error()));
  }
}

bool CharacterCanvas::PickPartAt(QPointF at) {
  assert(std::isfinite(at.x()));
  assert(session_ != nullptr);
  const Character* character = Shown();
  if (character == nullptr) {
    return false;
  }
  const std::optional<PartId> hit =
      HitPart(*character, Posed(), ToCharacter(at));
  if (!hit.has_value()) {
    return false;
  }
  session_->SetPart(*hit);
  return true;
}

void CharacterCanvas::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  setFocus();
  if (camera_.Press(*event)) {
    return;
  }
  if (event->button() != Qt::LeftButton || Shown() == nullptr) {
    return;
  }
  press_ = event->position();
  last_ = press_;
  now_ = press_;
  switch (session_->tool()) {
    case CharacterTool::kPick:
      PressPick(event);
      break;
    case CharacterTool::kMovePart:
      if (!session_->part().IsValid() && !PickPartAt(press_)) {
        return;
      }
      drag_ = Drag::kPart;
      OpenScope(tr("Move part"));
      break;
    case CharacterTool::kAddPoint:
      PressAdd(event);
      break;
    case CharacterTool::kFreehand:
      drag_ = Drag::kStroke;
      stroke_ = {press_};
      break;
    case CharacterTool::kOval:
    case CharacterTool::kRectangle:
      drag_ = Drag::kBoxShape;
      break;
    case CharacterTool::kCurvature: {
      const Character* character = Shown();
      const CharacterPart* part = FindPart(*character, session_->part());
      const PointId under = PointAt(Posed(), press_);
      if (under.IsValid() && !session_->points().contains(under)) {
        session_->SetPoints({under});
      }
      if (part == nullptr || session_->points().empty()) {
        Report(tr("Pick points to round or sharpen."));
        return;
      }
      const VectorPoint* first =
          FindPoint(part->drawing, *session_->points().begin());
      start_value_ = first != nullptr ? first->curvature : kSmoothCurvature;
      drag_ = Drag::kCurvature;
      OpenScope(tr("Curvature"));
      break;
    }
    case CharacterTool::kFill:
      Fill(press_);
      break;
    case CharacterTool::kStrength: {
      const auto bone = BoneAt(Posed(), press_);
      if (!bone.has_value()) {
        Report(tr("Press on a bone and drag to set how far it pulls."));
        return;
      }
      session_->SetBone(bone->first);
      drag_bone_ = bone->first;
      drag_ = Drag::kStrength;
      OpenScope(tr("Bone strength"));
      break;
    }
    case CharacterTool::kBindPoints:
      PressBind(event);
      break;
    case CharacterTool::kTryBones: {
      const auto bone = BoneAt(Posed(), press_);
      if (!bone.has_value()) {
        return;
      }
      session_->SetBone(bone->first);
      drag_bone_ = bone->first;
      drag_ = Drag::kTry;
      if (IsPosingStop()) {
        OpenScope(tr("Pose action stop"));
      } else {
        trying_ = ShownPose();
      }
      break;
    }
  }
  update();
}

void CharacterCanvas::PressPick(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  const Character* character = Shown();
  const PosedCharacter posed = Posed();
  const CharacterPart* part = FindPart(*character, session_->part());
  const auto bone = BoneAt(posed, press_);
  const bool is_bone_part = part != nullptr && part->kind == PartKind::kBone;
  if (bone.has_value() && (is_bone_part || part == nullptr ||
                           part->kind != PartKind::kVector ||
                           !PointAt(posed, press_).IsValid())) {
    session_->SetBone(bone->first);
    drag_bone_ = bone->first;
    drag_ = bone->second ? Drag::kBoneTip : Drag::kBoneWhole;
    if (IsPosingStop()) {
      drag_ = Drag::kTry;
      OpenScope(tr("Pose action stop"));
      return;
    }
    OpenScope(bone->second ? tr("Turn bone") : tr("Move bone"));
    return;
  }
  const PointId point = PointAt(posed, press_);
  if (point.IsValid()) {
    const bool is_picked = session_->points().contains(point);
    const std::optional<PickMode> mode =
        ClickMode(event->modifiers(), is_picked);
    if (mode.has_value()) {
      session_->SetPoints(Combine(session_->points(), {point}, *mode));
    }
    drag_ = Drag::kPoints;
    OpenScope(IsPosingStop() ? tr("Pose action stop") : tr("Move points"));
    return;
  }
  if (part != nullptr && part->kind == PartKind::kVector) {
    const PosedPart* shown = PickedShown(posed);
    const int shape =
        shown != nullptr
            ? ShapeAt(part->drawing, shown->points, ToCharacter(press_),
                      kClickSlop / Scale())
            : -1;
    if (shape >= 0) {
      session_->SetShape(shape);
      std::set<PointId> ids;
      for (int curve : part->drawing.shapes[static_cast<size_t>(shape)].curves) {
        for (PointId id : part->drawing.curves[static_cast<size_t>(curve)].points) {
          ids.insert(id);
        }
      }
      session_->SetPoints(Combine(session_->points(), ids,
                                  BoxMode(event->modifiers())));
      drag_ = Drag::kPoints;
      OpenScope(IsPosingStop() ? tr("Pose action stop") : tr("Move shape"));
      return;
    }
  }
  const bool is_plain = event->modifiers() == Qt::NoModifier;
  if (is_plain && PickPartAt(press_)) {
    return;
  }
  drag_ = Drag::kBox;
}

void CharacterCanvas::PressAdd(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* part = FindPart(*character, session_->part());
  if (part == nullptr) {
    Report(tr("Pick a drawing or bone part on the left first."));
    return;
  }
  if (part->kind == PartKind::kBone) {
    drag_ = Drag::kAddBone;
    return;
  }
  if (part->kind != PartKind::kVector) {
    Report(tr("Points go on a drawing part, bones on a bone part."));
    return;
  }
  const PointId under = PointAt(Posed(), press_);
  const bool is_lone_pick = session_->points().size() == 1;
  from_point_ = under.IsValid()  ? under
                : is_lone_pick ? *session_->points().begin()
                               : PointId();
  drag_ = Drag::kAddLine;
}

void CharacterCanvas::PressBind(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  const PosedCharacter posed = Posed();
  const auto bone = BoneAt(posed, press_);
  const PointId point = PointAt(posed, press_);
  if (bone.has_value() && !point.IsValid()) {
    session_->SetBone(bone->first);
    return;
  }
  if (!session_->bone().IsValid()) {
    Report(tr("Click a bone first, then the points it should carry."));
    return;
  }
  if (point.IsValid()) {
    const std::vector<PointId> ids = {point};
    const bool is_unbind = event->modifiers().testFlag(Qt::ControlModifier);
    const auto done = characters_->BindPoints(
        session_->character(), session_->part(), ids,
        is_unbind ? BoneId() : session_->bone());
    Report(done ? QString() : ProblemOf(done.error()));
    return;
  }
  drag_ = Drag::kBox;
}

void CharacterCanvas::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  if (camera_.Move(*event)) {
    update();
    return;
  }
  if (drag_ == Drag::kNone) {
    return;
  }
  now_ = event->position();
  MoveDrag(now_, event->modifiers());
  last_ = now_;
  update();
}

void CharacterCanvas::MoveDrag(QPointF at, Qt::KeyboardModifiers mods) {
  assert(std::isfinite(at.x()));
  assert(session_ != nullptr);
  const QString& name = session_->character();
  const Character* character = Shown();
  switch (drag_) {
    case Drag::kPoints: {
      const QPointF step = IntoPart(session_->part(), last_, at);
      const std::vector<PointId> ids(session_->points().begin(),
                                     session_->points().end());
      if (IsPosingStop()) {
        const SmartAction* action = FindAction(*character, session_->action_bone());
        PoseStop(WithPointsShift(
            action->stops[static_cast<size_t>(session_->action_stop())].pose,
            session_->part(), ids, step));
        break;
      }
      const auto done = characters_->TransformPoints(
          name, session_->part(), ids,
          QTransform::fromTranslate(step.x(), step.y()));
      if (!done) Report(ProblemOf(done.error()));
      break;
    }
    case Drag::kBoneTip:
    case Drag::kBoneWhole: {
      const CharacterPart* bones = BonesShown();
      const Bone* bone = FindBone(*character, drag_bone_);
      if (bones == nullptr || bone == nullptr) break;
      const QPointF step = IntoBones(*bones, at) - IntoBones(*bones, last_);
      const QPointF tip = TipOf(*bone);
      const bool is_tip = drag_ == Drag::kBoneTip;
      const auto done = characters_->SetBoneRest(
          name, drag_bone_, is_tip ? bone->base : bone->base + step,
          tip + step);
      if (!done) Report(ProblemOf(done.error()));
      break;
    }
    case Drag::kPart: {
      const CharacterPart* part = FindPart(*character, session_->part());
      if (part == nullptr) break;
      const PosedCharacter posed = Posed();
      const auto parent = posed.spaces.find(part->parent);
      const QTransform back = parent == posed.spaces.end()
                                  ? QTransform()
                                  : parent->second.inverted();
      const QPointF step =
          back.map(ToCharacter(at)) - back.map(ToCharacter(last_));
      if (IsPosingStop()) {
        const SmartAction* action = FindAction(*character, session_->action_bone());
        PoseStop(WithPartShift(
            action->stops[static_cast<size_t>(session_->action_stop())].pose,
            part->id, step));
        break;
      }
      CharacterPart rest = *part;
      rest.offset += step;
      const auto done = characters_->SetPartRest(name, part->id, rest);
      if (!done) Report(ProblemOf(done.error()));
      break;
    }
    case Drag::kStroke:
      stroke_.push_back(at);
      break;
    case Drag::kCurvature: {
      const double value =
          std::clamp(start_value_ + (at.x() - press_.x()) / 200.0, 0.0, kMaxCurvature);
      const std::vector<PointId> ids(session_->points().begin(),
                                     session_->points().end());
      const auto done = characters_->SetCurvature(
          name, session_->part(), ids,
          mods.testFlag(Qt::ShiftModifier) ? std::round(value * 10.0) / 10.0
                                           : value);
      if (!done) Report(ProblemOf(done.error()));
      break;
    }
    case Drag::kStrength: {
      const Bone* bone = FindBone(*character, drag_bone_);
      const CharacterPart* bones = BonesShown();
      if (bone == nullptr || bones == nullptr) break;
      const QPointF at_bones = IntoBones(*bones, at);
      const QPointF tip = TipOf(*bone);
      const QPointF along = tip - bone->base;
      const double length2 = QPointF::dotProduct(along, along);
      const double t =
          length2 > 0.0 ? std::clamp(QPointF::dotProduct(at_bones - bone->base,
                                                         along) / length2,
                                     0.0, 1.0)
                        : 0.0;
      Bone changed = *bone;
      changed.strength = QLineF(at_bones, bone->base + t * along).length();
      const auto done = characters_->SetBone(name, changed);
      if (!done) Report(ProblemOf(done.error()));
      break;
    }
    case Drag::kTry: {
      const CharacterPose shown = ShownPose();
      const CharacterPose reached =
          WithBoneDrag(*character, shown, drag_bone_, ToCharacter(at),
                       mods.testFlag(Qt::ShiftModifier));
      if (!IsPosingStop()) {
        trying_ = reached;
        break;
      }
      const SmartAction* action = FindAction(*character, session_->action_bone());
      CharacterPose stop =
          action->stops[static_cast<size_t>(session_->action_stop())].pose;
      for (const auto& [id, bone] : reached.bones) {
        if (id != session_->action_bone()) {
          stop.bones[id].turn = bone.turn;
        }
      }
      PoseStop(stop);
      break;
    }
    default:
      break;
  }
}

void CharacterCanvas::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  if (camera_.Release(*event)) {
    return;
  }
  if (drag_ == Drag::kNone) {
    return;
  }
  FinishDrag(event->position(), event->modifiers());
  update();
}

void CharacterCanvas::FinishDrag(QPointF at, Qt::KeyboardModifiers mods) {
  assert(std::isfinite(at.x()));
  assert(session_ != nullptr);
  const Drag was = drag_;
  drag_ = Drag::kNone;
  const bool is_click = QLineF(press_, at).length() < kClickSlop;
  switch (was) {
    case Drag::kBox: {
      const PosedCharacter posed = Posed();
      const PosedPart* shown = PickedShown(posed);
      const Character* character = Shown();
      const CharacterPart* part =
          character != nullptr ? FindPart(*character, session_->part()) : nullptr;
      if (is_click) {
        if (mods == Qt::NoModifier) {
          session_->SetPoints({});
          session_->SetShape(-1);
          session_->SetBone(BoneId());
        }
        break;
      }
      if (shown == nullptr || part == nullptr || part->kind != PartKind::kVector) {
        break;
      }
      const QRectF box = QRectF(press_, at).normalized();
      const QTransform view = View();
      std::set<PointId> inside;
      for (size_t i = 0; i < part->drawing.points.size(); ++i) {
        if (box.contains(view.map(shown->points[i]))) {
          inside.insert(part->drawing.points[i].id);
        }
      }
      if (session_->tool() == CharacterTool::kBindPoints) {
        const std::vector<PointId> ids(inside.begin(), inside.end());
        const bool is_unbind = mods.testFlag(Qt::ControlModifier);
        const auto done = characters_->BindPoints(
            session_->character(), session_->part(), ids,
            is_unbind ? BoneId() : session_->bone());
        Report(done ? QString() : ProblemOf(done.error()));
        break;
      }
      session_->SetPoints(Combine(session_->points(), inside, BoxMode(mods)));
      break;
    }
    case Drag::kPoints: {
      CloseScope(!is_click);
      const PointId point = PointAt(Posed(), at);
      const bool narrows = is_click && mods == Qt::NoModifier && point.IsValid();
      if (narrows && session_->shape() < 0) {
        session_->SetPoints({point});
      }
      break;
    }
    case Drag::kStroke:
      FinishStroke();
      break;
    case Drag::kBoxShape:
      if (!is_click) {
        FinishBoxShape(at, mods.testFlag(Qt::ShiftModifier));
      }
      break;
    case Drag::kAddLine:
      FinishAddLine(at);
      break;
    case Drag::kAddBone:
      if (is_click) {
        Report(tr("Drag from where the bone starts to its tip."));
      } else {
        FinishAddBone(at);
      }
      break;
    case Drag::kTry:
      trying_.reset();
      CloseScope(true);
      break;
    default:
      CloseScope(!is_click);
      break;
  }
  stroke_.clear();
}

void CharacterCanvas::FinishStroke() {
  assert(session_ != nullptr);
  assert(characters_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* part =
      character != nullptr ? FindPart(*character, session_->part()) : nullptr;
  if (part == nullptr || part->kind != PartKind::kVector) {
    Report(tr("Pick a drawing part on the left to draw on."));
    return;
  }
  if (stroke_.size() < 2) {
    return;
  }
  const std::vector<QPointF> fitted = SimplifyStroke(stroke_, kStrokeTolerance);
  const bool is_closed = fitted.size() >= 3 &&
                         QLineF(stroke_.front(), stroke_.back()).length() <
                             kCloseReach;
  const PosedCharacter posed = Posed();
  const auto space = posed.spaces.find(part->id);
  const QTransform back = space == posed.spaces.end() ? QTransform()
                                                      : space->second.inverted();
  std::vector<QPointF> places;
  for (size_t i = 0; i < fitted.size(); ++i) {
    if (is_closed && i + 1 == fitted.size()) {
      break;
    }
    places.push_back(back.map(ToCharacter(fitted[i])));
  }
  const auto ids = characters_->AddCurve(session_->character(), part->id,
                                         places, is_closed, kSmoothCurvature);
  if (!ids) {
    Report(ProblemOf(ids.error()));
    return;
  }
  VectorShape style = session_->style();
  style.has_fill = style.has_fill && is_closed;
  style.has_stroke = style.has_stroke || !is_closed;
  const auto shape =
      characters_->MakeShape(session_->character(), part->id, *ids, style);
  session_->SetPoints(std::set<PointId>(ids->begin(), ids->end()));
  session_->SetShape(shape ? *shape : -1);
  Report(QString());
}

void CharacterCanvas::FinishBoxShape(QPointF at, bool is_even) {
  assert(session_ != nullptr);
  assert(std::isfinite(at.x()));
  const Character* character = Shown();
  const CharacterPart* part =
      character != nullptr ? FindPart(*character, session_->part()) : nullptr;
  if (part == nullptr || part->kind != PartKind::kVector) {
    Report(tr("Pick a drawing part on the left to draw on."));
    return;
  }
  QPointF end = at;
  if (is_even) {
    const double side = std::max(std::abs(at.x() - press_.x()),
                                 std::abs(at.y() - press_.y()));
    end = press_ + QPointF(at.x() < press_.x() ? -side : side,
                           at.y() < press_.y() ? -side : side);
  }
  const PosedCharacter posed = Posed();
  const auto space = posed.spaces.find(part->id);
  const QTransform back = space == posed.spaces.end() ? QTransform()
                                                      : space->second.inverted();
  const QRectF box(back.map(ToCharacter(press_)), back.map(ToCharacter(end)));
  const bool is_oval = session_->tool() == CharacterTool::kOval;
  const std::vector<QPointF> places =
      is_oval ? OvalPlaces(box.normalized()) : RectanglePlaces(box);
  const auto ids = characters_->AddCurve(
      session_->character(), part->id, places, true,
      is_oval ? kSmoothCurvature : 0.0);
  if (!ids) {
    Report(ProblemOf(ids.error()));
    return;
  }
  const auto shape = characters_->MakeShape(session_->character(), part->id,
                                            *ids, session_->style());
  session_->SetPoints(std::set<PointId>(ids->begin(), ids->end()));
  session_->SetShape(shape ? *shape : -1);
}

void CharacterCanvas::FinishAddLine(QPointF at) {
  assert(session_ != nullptr);
  assert(std::isfinite(at.x()));
  const Character* character = Shown();
  const CharacterPart* part = FindPart(*character, session_->part());
  const PosedCharacter posed = Posed();
  const auto space = posed.spaces.find(part->id);
  const QTransform back = space == posed.spaces.end() ? QTransform()
                                                      : space->second.inverted();
  const QPointF end = back.map(ToCharacter(at));
  const bool is_click = QLineF(press_, at).length() < kClickSlop;
  if (is_click && !from_point_.IsValid()) {
    Report(tr("Drag to draw a line; click next to a picked end to go on."));
    return;
  }
  if (is_click || from_point_.IsValid()) {
    const PointId from = from_point_;
    const auto added =
        characters_->ExtendFrom(session_->character(), part->id, from, end);
    if (!added) {
      Report(ProblemOf(added.error()));
      return;
    }
    const PointId onto = PointAt(posed, at);
    if (onto.IsValid() && onto != from && !is_click) {
      const auto welded =
          characters_->Weld(session_->character(), part->id, onto, *added);
      session_->SetPoints({welded ? onto : *added});
      return;
    }
    session_->SetPoints({*added});
    return;
  }
  const std::vector<QPointF> places = {back.map(ToCharacter(press_)), end};
  const auto ids = characters_->AddCurve(session_->character(), part->id,
                                         places, false, kSmoothCurvature);
  if (!ids) {
    Report(ProblemOf(ids.error()));
    return;
  }
  session_->SetPoints({ids->back()});
}

void CharacterCanvas::FinishAddBone(QPointF at) {
  assert(session_ != nullptr);
  assert(std::isfinite(at.x()));
  const CharacterPart* bones = BonesShown();
  if (bones == nullptr) {
    Report(tr("Pick a bone part on the left to add bones."));
    return;
  }
  QPointF base = IntoBones(*bones, press_);
  const auto grabbed = BoneAt(Posed(), press_);
  BoneId parent = session_->bone();
  if (grabbed.has_value() && grabbed->second) {
    const PosedCharacter rest = PoseCharacter(*Shown(), CharacterPose{});
    for (const PosedBone& bone : rest.bones) {
      if (bone.id == grabbed->first) {
        const auto space = rest.spaces.find(bones->id);
        base = space == rest.spaces.end() ? bone.tip
                                          : space->second.inverted().map(bone.tip);
        parent = bone.id;
      }
    }
  }
  const auto added = characters_->AddBone(session_->character(), bones->id,
                                          base, IntoBones(*bones, at), parent);
  if (!added) {
    Report(ProblemOf(added.error()));
    return;
  }
  session_->SetBone(*added);
  Report(QString());
}

void CharacterCanvas::Fill(QPointF at) {
  assert(session_ != nullptr);
  assert(std::isfinite(at.x()));
  const Character* character = Shown();
  if (!PickPartAt(at)) {
    Report(tr("Click a shape to paint it with the current colors."));
    return;
  }
  const CharacterPart* part = FindPart(*character, session_->part());
  const PosedCharacter posed = Posed();
  const PosedPart* shown = PickedShown(posed);
  if (part == nullptr || shown == nullptr || part->kind != PartKind::kVector) {
    return;
  }
  const int shape =
      ShapeAt(part->drawing, shown->points, ToCharacter(at), kClickSlop / Scale());
  if (shape < 0) {
    Report(tr("Click a shape to paint it with the current colors."));
    return;
  }
  VectorShape style = session_->style();
  const VectorShape& old = part->drawing.shapes[static_cast<size_t>(shape)];
  style.has_fill = style.has_fill && old.has_fill;
  const auto done = characters_->SetShapeStyle(session_->character(), part->id,
                                               shape, style);
  session_->SetShape(shape);
  Report(done ? QString() : ProblemOf(done.error()));
}

void CharacterCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  if (event->button() == Qt::LeftButton && session_->tool() == CharacterTool::kPick) {
    PickPartAt(event->position());
  }
}

void CharacterCanvas::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(Scale() > 0.0);
  const double fit = std::min(width(), height()) / 1200.0;
  camera_.Wheel(*event, std::max(fit, 0.01), QSizeF(1200.0, 1200.0),
                QSizeF(size()));
  update();
}

void CharacterCanvas::DeletePicked() {
  assert(session_ != nullptr);
  assert(characters_ != nullptr);
  const QString& name = session_->character();
  if (!session_->points().empty()) {
    const std::vector<PointId> ids(session_->points().begin(),
                                   session_->points().end());
    const auto done = characters_->DeletePoints(name, session_->part(), ids);
    session_->SetPoints({});
    session_->SetShape(-1);
    Report(done ? QString() : ProblemOf(done.error()));
    return;
  }
  if (session_->shape() >= 0) {
    const auto done =
        characters_->RemoveShape(name, session_->part(), session_->shape());
    session_->SetShape(-1);
    Report(done ? QString() : ProblemOf(done.error()));
    return;
  }
  if (session_->bone().IsValid()) {
    const std::vector<BoneId> ids = {session_->bone()};
    const auto done = characters_->RemoveBones(name, ids);
    session_->SetBone(BoneId());
    Report(done ? QString() : ProblemOf(done.error()));
  }
}

void CharacterCanvas::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(session_ != nullptr);
  if (event->key() == Qt::Key_Escape && drag_ != Drag::kNone) {
    CloseScope(false);
    drag_ = Drag::kNone;
    trying_.reset();
    stroke_.clear();
    update();
    return;
  }
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    DeletePicked();
    return;
  }
  const bool is_select_all = event->matches(QKeySequence::SelectAll);
  if (is_select_all) {
    const Character* character = Shown();
    const CharacterPart* part =
        character != nullptr ? FindPart(*character, session_->part()) : nullptr;
    if (part != nullptr) {
      std::set<PointId> all;
      for (const VectorPoint& point : part->drawing.points) all.insert(point.id);
      session_->SetPoints(std::move(all));
    }
    return;
  }
  QWidget::keyPressEvent(event);
}

}  // namespace snapper
