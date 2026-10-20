#include "ui/character_canvas.h"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/vector_path.h"
#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "render/character_paint.h"
#include "ui/bone_glyph.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The square the canvas fits at zoom 1, in character pixels.
constexpr double kFitSide = 1200.0;
// Handles look small and grab big.
constexpr double kHandleSize = 7.0;
constexpr double kGrabReach = 9.0;

QColor Faded(QColor color, int alpha) {
  assert(alpha >= 0 && alpha <= 255);
  assert(color.isValid());
  color.setAlpha(alpha);
  return color;
}

}  // namespace

CharacterCanvas::CharacterCanvas(HistoryManager* history,
                                 CharacterManager* characters,
                                 CharacterSession* session, QWidget* parent)
    : QWidget(parent),
      history_(history),
      characters_(characters),
      session_(session),
      camera_(this) {
  assert(history_ != nullptr && characters_ != nullptr);
  assert(session_ != nullptr);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumSize(320, 240);
  theme::SetLevel(this, theme::kWell);
  connect(history_, &HistoryManager::Changed, this,
          qOverload<>(&QWidget::update));
  connect(session_, &CharacterSession::Changed, this,
          qOverload<>(&QWidget::update));
}

const Character* CharacterCanvas::Shown() const {
  assert(history_ != nullptr);
  assert(session_ != nullptr);
  return session_->character().isEmpty()
             ? nullptr
             : FindCharacter(history_->current(), session_->character());
}

bool CharacterCanvas::IsPosingStop() const {
  assert(session_ != nullptr);
  assert(session_->action_stop() >= -1);
  return session_->action_bone().IsValid() && session_->action_stop() >= 0;
}

CharacterPose CharacterCanvas::ShownPose() const {
  assert(session_ != nullptr);
  assert(std::isfinite(session_->action_turn()));
  if (trying_.has_value()) {
    return *trying_;
  }
  const Character* character = Shown();
  if (character == nullptr || !session_->action_bone().IsValid()) {
    return CharacterPose{};
  }
  CharacterPose hand;
  hand.bones[session_->action_bone()].turn = session_->action_turn();
  return WithActions(*character, hand);
}

void CharacterCanvas::FitAll() {
  assert(session_ != nullptr);
  assert(width() >= 0);
  camera_.Reset();
  update();
}

double CharacterCanvas::Scale() const {
  assert(width() >= 0 && height() >= 0);
  const double fit = std::min(width(), height()) / kFitSide;
  return camera_.ScaleFor(std::max(fit, 0.01));
}

QTransform CharacterCanvas::View() const {
  assert(width() >= 0 && height() >= 0);
  const double scale = Scale();
  const QPointF corner = camera_.CornerFor(
      scale, QSizeF(kFitSide, kFitSide), QSizeF(size()));
  QTransform view;
  view.translate(corner.x(), corner.y());
  view.scale(scale, scale);
  view.translate(kFitSide / 2.0, kFitSide / 2.0);
  return view;
}

QPointF CharacterCanvas::ToCharacter(QPointF widget) const {
  assert(std::isfinite(widget.x()));
  assert(Scale() > 0.0);
  return View().inverted().map(widget);
}

PosedCharacter CharacterCanvas::Posed() const {
  const Character* character = Shown();
  assert(session_ != nullptr);
  if (character == nullptr) {
    return PosedCharacter{};
  }
  return PoseCharacter(*character, ShownPose());
}

const PosedPart* CharacterCanvas::PickedShown(
    const PosedCharacter& posed) const {
  assert(session_ != nullptr);
  assert(posed.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  const auto found = std::ranges::find_if(posed.parts, [&](const PosedPart& p) {
    return p.id == session_->part();
  });
  return found == posed.parts.end() ? nullptr : &*found;
}

const CharacterPart* CharacterCanvas::BonesShown() const {
  const Character* character = Shown();
  assert(session_ != nullptr);
  if (character == nullptr || !session_->part().IsValid()) {
    return nullptr;
  }
  const CharacterPart* part = FindPart(*character, session_->part());
  if (part != nullptr && part->kind == PartKind::kBone) {
    return part;
  }
  return BoneParentOf(*character, session_->part());
}

QPointF CharacterCanvas::IntoPart(PartId part, QPointF from, QPointF to) const {
  assert(std::isfinite(from.x()) && std::isfinite(to.x()));
  assert(part.value() >= 0);
  const QPointF step = ToCharacter(to) - ToCharacter(from);
  const PosedCharacter posed = Posed();
  const auto found = std::ranges::find_if(
      posed.parts, [part](const PosedPart& p) { return p.id == part; });
  if (found == posed.parts.end()) {
    return step;
  }
  const QTransform back = found->to_character.inverted();
  return back.map(step) - back.map(QPointF());
}

QPointF CharacterCanvas::IntoBones(const CharacterPart& bones,
                                   QPointF widget) const {
  assert(std::isfinite(widget.x()));
  assert(bones.kind == PartKind::kBone);
  const Character* character = Shown();
  const QPointF at = ToCharacter(widget);
  if (character == nullptr) {
    return at;
  }
  const PosedCharacter rest = PoseCharacter(*character, CharacterPose{});
  const auto space = rest.spaces.find(bones.id);
  return space == rest.spaces.end() ? at : space->second.inverted().map(at);
}

void CharacterCanvas::PaintGrid(QPainter* painter) const {
  assert(painter != nullptr);
  assert(Scale() > 0.0);
  const QTransform view = View();
  painter->save();
  painter->setTransform(view);
  const double step = Scale() < 0.4 ? 200.0 : 100.0;
  QPen line(Faded(theme::kFaceLight, 40), 0.0);
  painter->setPen(line);
  const QRectF seen = view.inverted().mapRect(QRectF(rect()));
  for (double x = std::floor(seen.left() / step) * step; x < seen.right();
       x += step) {
    painter->drawLine(QPointF(x, seen.top()), QPointF(x, seen.bottom()));
  }
  for (double y = std::floor(seen.top() / step) * step; y < seen.bottom();
       y += step) {
    painter->drawLine(QPointF(seen.left(), y), QPointF(seen.right(), y));
  }
  painter->setPen(QPen(Faded(theme::kPick, 140), 0.0));
  painter->drawLine(QPointF(-20, 0), QPointF(20, 0));
  painter->drawLine(QPointF(0, -20), QPointF(0, 20));
  painter->restore();
}

void CharacterCanvas::paintEvent(QPaintEvent*) {
  assert(session_ != nullptr);
  assert(history_ != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kWell);
  PaintGrid(&painter);
  const Character* character = Shown();
  if (character == nullptr) {
    painter.setPen(theme::kTextHint);
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
                     tr("Make a character with New, or pick one on the "
                        "left, to draw and rig it here."));
    return;
  }
  const PosedCharacter posed = Posed();
  PaintCharacter(*character, posed, View(), &cache_, &painter);
  painter.setRenderHint(QPainter::Antialiasing);
  PaintPicked(&painter, posed);
  PaintBones(&painter, posed);
  PaintTool(&painter);
  if (IsPosingStop()) {
    painter.setPen(theme::kPick);
    painter.drawRect(rect().adjusted(1, 1, -2, -2));
    painter.drawText(rect().adjusted(10, 8, -10, -8),
                     Qt::AlignTop | Qt::AlignLeft,
                     tr("Posing a smart bone stop: what you move here plays "
                        "as the bone turns."));
  }
}

void CharacterCanvas::PaintPicked(QPainter* painter,
                                  const PosedCharacter& posed) const {
  assert(painter != nullptr);
  assert(session_ != nullptr);
  const Character* character = Shown();
  const PosedPart* shown = PickedShown(posed);
  if (character == nullptr || shown == nullptr) {
    return;
  }
  const CharacterPart* part = FindPart(*character, shown->id);
  const QTransform view = View();
  painter->save();
  if (part->kind == PartKind::kImage) {
    QPainterPath box;
    box.addRect(QRectF(QPointF(), part->image_size));
    painter->setPen(QPen(theme::kPick, 1.5));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath((shown->to_character * view).map(box));
    painter->restore();
    return;
  }
  painter->setTransform(view);
  painter->setPen(QPen(Faded(theme::kPick, 150), 0.0));
  painter->setBrush(Qt::NoBrush);
  for (const VectorCurve& curve : part->drawing.curves) {
    painter->drawPath(CurvePath(part->drawing, curve, shown->points));
  }
  const int shape = session_->shape();
  if (shape >= 0 && static_cast<size_t>(shape) < part->drawing.shapes.size()) {
    painter->setPen(QPen(theme::kPick, 2.0 / Scale()));
    painter->drawPath(ShapeStrokePath(
        part->drawing, part->drawing.shapes[static_cast<size_t>(shape)],
        shown->points));
  }
  painter->resetTransform();
  const BoneId bone = session_->bone();
  for (size_t i = 0; i < part->drawing.points.size(); ++i) {
    const VectorPoint& point = part->drawing.points[i];
    const QPointF at = view.map(shown->points[i]);
    const bool is_picked = session_->points().contains(point.id);
    const auto bound = part->point_bones.find(point.id);
    const bool is_bound_here = bone.IsValid() && bound != part->point_bones.end() &&
                               bound->second == bone;
    const QColor fill = is_picked       ? theme::kPick
                        : is_bound_here ? theme::kCollider
                                        : theme::kHandle;
    painter->setPen(QPen(theme::kShadow, 1.0));
    painter->setBrush(fill);
    const double side = is_picked ? kHandleSize + 2.0 : kHandleSize;
    const QRectF square(at.x() - side / 2.0, at.y() - side / 2.0, side, side);
    if (point.curvature <= 0.0) {
      painter->drawRect(square);
    } else {
      painter->drawEllipse(square);
    }
  }
  painter->restore();
}

void CharacterCanvas::PaintBones(QPainter* painter,
                                 const PosedCharacter& posed) const {
  assert(painter != nullptr);
  assert(session_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* bones = BonesShown();
  if (character == nullptr || bones == nullptr) {
    return;
  }
  const QTransform view = View();
  const bool shows_reach = session_->tool() == CharacterTool::kStrength;
  painter->save();
  for (const PosedBone& bone : posed.bones) {
    if (BoneOwner(*character, bone.id) != bones) {
      continue;
    }
    const Bone* rest = FindBone(*character, bone.id);
    const QPointF base = view.map(bone.base);
    const QPointF tip = view.map(bone.tip);
    const bool is_picked = bone.id == session_->bone();
    if ((shows_reach || is_picked) && !rest->is_smart) {
      QPainterPathStroker reach;
      reach.setWidth(rest->strength * 2.0 * Scale());
      reach.setCapStyle(Qt::RoundCap);
      QPainterPath spine(base);
      spine.lineTo(tip);
      painter->setPen(Qt::NoPen);
      painter->setBrush(Faded(is_picked ? theme::kPick : theme::kHandle, 34));
      painter->drawPath(reach.createStroke(spine));
    }
    const QColor body = is_picked        ? theme::kPick
                        : rest->is_smart ? theme::kCollider
                                         : theme::kFaceLight;
    PaintBoneGlyph(painter, base, tip, body);
  }
  painter->restore();
}

void CharacterCanvas::PaintTool(QPainter* painter) const {
  assert(painter != nullptr);
  assert(session_ != nullptr);
  painter->save();
  painter->setBrush(Qt::NoBrush);
  QPen pen(theme::kPick, 1.5);
  switch (drag_) {
    case Drag::kStroke: {
      painter->setPen(pen);
      QPolygonF line;
      for (const QPointF& at : stroke_) line << at;
      painter->drawPolyline(line);
      break;
    }
    case Drag::kBox:
      pen.setStyle(Qt::DashLine);
      painter->setPen(pen);
      painter->drawRect(QRectF(press_, now_).normalized());
      break;
    case Drag::kBoxShape: {
      painter->setPen(pen);
      const QRectF box = QRectF(press_, now_).normalized();
      if (session_->tool() == CharacterTool::kOval) {
        painter->drawEllipse(box);
      } else {
        painter->drawRect(box);
      }
      break;
    }
    case Drag::kMagnet: {
      // The reach goes with the cursor, as the points do.
      pen.setStyle(Qt::DashLine);
      painter->setPen(pen);
      const double radius = session_->magnet_radius();
      painter->drawEllipse(now_, radius, radius);
      break;
    }
    case Drag::kAddLine:
    case Drag::kAddBone:
      painter->setPen(pen);
      painter->drawLine(press_, now_);
      break;
    default:
      break;
  }
  painter->restore();
}

PointId CharacterCanvas::PointAt(const PosedCharacter& posed,
                                 QPointF at) const {
  assert(std::isfinite(at.x()));
  assert(session_ != nullptr);
  const Character* character = Shown();
  const PosedPart* shown = PickedShown(posed);
  if (character == nullptr || shown == nullptr ||
      shown->kind != PartKind::kVector) {
    return PointId();
  }
  const CharacterPart* part = FindPart(*character, shown->id);
  const QTransform view = View();
  PointId best;
  double best_gap = kGrabReach;
  for (size_t i = 0; i < part->drawing.points.size(); ++i) {
    const double gap = QLineF(view.map(shown->points[i]), at).length();
    if (gap <= best_gap) {
      best_gap = gap;
      best = part->drawing.points[i].id;
    }
  }
  return best;
}

std::optional<std::pair<BoneId, bool>> CharacterCanvas::BoneAt(
    const PosedCharacter& posed, QPointF at) const {
  assert(std::isfinite(at.x()));
  assert(session_ != nullptr);
  const Character* character = Shown();
  const CharacterPart* bones = BonesShown();
  if (character == nullptr || bones == nullptr) {
    return std::nullopt;
  }
  const QTransform view = View();
  std::optional<std::pair<BoneId, bool>> best;
  double best_gap = kGrabReach;
  for (const PosedBone& bone : posed.bones) {
    if (BoneOwner(*character, bone.id) != bones) {
      continue;
    }
    const QPointF base = view.map(bone.base);
    const QPointF tip = view.map(bone.tip);
    const double tip_gap = QLineF(tip, at).length();
    if (tip_gap <= best_gap) {
      best_gap = tip_gap;
      best = std::pair{bone.id, true};
      continue;
    }
    const QPointF along = tip - base;
    const double length2 = QPointF::dotProduct(along, along);
    const double t = length2 > 0.0
                         ? std::clamp(QPointF::dotProduct(at - base, along) /
                                          length2, 0.0, 1.0)
                         : 0.0;
    const double gap = QLineF(base + t * along, at).length();
    if (gap <= best_gap) {
      best_gap = gap;
      best = std::pair{bone.id, false};
    }
  }
  return best;
}

}  // namespace snapper
