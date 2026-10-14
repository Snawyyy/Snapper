#include "ui/comp_stage.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "anim/comp_knobs.h"
#include "anim/reel_timeline.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "render/stage_hit.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kMargin = 16.0;
constexpr int kMaxKept = 48;
constexpr double kArrowLength = 80.0;
constexpr double kHandleSize = 9.0;
// Handles look small and grab big.
constexpr double kGrabRadius = 14.0;
constexpr int kChipMs = 1200;

// The reel playhead, and the shot clip showing on top there.
struct Here final {
  Frame reel;
  ReelShot at;
};

Here Locating(const Managers& managers) {
  Here here;
  here.reel = managers.playback->FrameOn(Timeline::kReel);
  here.at = ShotAtReel(managers.history->current(), here.reel);
  return here;
}

// The knob the arrow handle turns, if the effect has one shown.
std::optional<CompKnob> ArrowKnob(const CompEffect& effect) {
  const auto spec = KnobSpecOf(effect, CompKnob::kAngle);
  const bool is_turnable = spec.has_value() && spec->is_angle;
  const bool is_burst =
      effect.kind == CompKind::kSpeedLines && effect.style == 0;
  return is_turnable && !is_burst ? std::optional(CompKnob::kAngle)
                                  : std::nullopt;
}

double AngleOf(QPointF from, QPointF to) {
  const QPointF way = to - from;
  const double degrees =
      std::atan2(way.x(), -way.y()) * 180.0 / std::numbers::pi;
  return degrees < 0.0 ? degrees + 360.0 : degrees;
}

}  // namespace

CompStage::CompStage(const Managers& managers)
    : managers_(managers), camera_(this) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  setMinimumSize(240, 160);
  chip_timer_.setSingleShot(true);
  connect(&chip_timer_, &QTimer::timeout, this, [this] {
    chip_.clear();
    update();
  });
  const auto forget = [this] {
    Forget();
    update();
  };
  connect(managers_.history, &HistoryManager::Changed, this, forget);
  connect(managers_.comp, &CompManager::Changed, this, forget);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          [this] { update(); });
  connect(managers_.selection, &SelectionManager::Changed, this,
          [this] { update(); });
}

void CompStage::ShowChip(const QString& text) {
  assert(!text.isEmpty());
  chip_ = text;
  chip_timer_.start(kChipMs);
  update();
}

double CompStage::FitScale() const {
  const CanvasSize& canvas = managers_.history->current().canvas;
  assert(canvas.width > 0 && canvas.height > 0);
  const double fit = std::min((width() - 2.0 * kMargin) / canvas.width,
                              (height() - 2.0 * kMargin) / canvas.height);
  return std::clamp(fit, 0.05, 1.0);
}

double CompStage::Scale() const { return camera_.ScaleFor(FitScale()); }

double CompStage::RenderScale() const {
  // Never drawn finer than one frame pixel per pixel: zoomed in past
  // that the picture is magnified, not drawn again bigger.
  return std::min(Scale(), std::max(1.0, FitScale()));
}

QRectF CompStage::FrameRect() const {
  const CanvasSize& canvas = managers_.history->current().canvas;
  const double scale = Scale();
  const QSizeF size(std::max(1.0, std::round(canvas.width * scale)),
                    std::max(1.0, std::round(canvas.height * scale)));
  const QPointF corner = camera_.CornerFor(
      scale, QSizeF(canvas.width, canvas.height), QSizeF(this->size()));
  return QRectF(QPointF(std::round(corner.x()), std::round(corner.y())), size);
}

Frame CompStage::Local() const { return Locating(managers_).at.local; }

void CompStage::Forget() {
  kept_.clear();
  assert(kept_.empty());
}

QImage CompStage::Picture() {
  assert(managers_.comp != nullptr);
  const Here here = Locating(managers_);
  const Project& project = managers_.history->current();
  const bool is_empty = IsReelEmpty(project.reel);
  if (is_empty) {
    return QImage();
  }
  const double render_scale = RenderScale();
  const bool is_new_scale = render_scale != kept_scale_;
  if (is_new_scale) {
    Forget();
    kept_scale_ = render_scale;
  }
  const auto found = kept_.find(here.reel.index());
  if (found != kept_.end()) {
    return found->second;
  }
  renderer_.SetCompShow(managers_.comp->ShowFor(managers_.selection->shot()));
  QImage picture =
      renderer_.RenderReel(project, here.reel, render_scale, &videos_);
  managers_.comp->NoteCosts(renderer_.comp_report());
  const bool is_full = kept_.size() >= static_cast<size_t>(kMaxKept);
  if (is_full) {
    // Drop the frame farthest from this one.
    const int now = here.reel.index();
    const auto far = std::ranges::max_element(
        kept_, {}, [now](const auto& item) { return std::abs(item.first - now); });
    kept_.erase(far);
  }
  kept_[here.reel.index()] = picture;
  return picture;
}

QColor CompStage::ColorUnderCursor() {
  const QPointF at = mapFromGlobal(QCursor::pos());
  const QRectF frame = FrameRect();
  const bool is_on_frame = frame.contains(at);
  const Here here = Locating(managers_);
  if (!is_on_frame || IsReelEmpty(managers_.history->current().reel)) {
    return QColor();
  }
  CompShow off;
  off.is_off = true;
  renderer_.SetCompShow(off);
  const QImage plain = renderer_.RenderReel(
      managers_.history->current(), here.reel, RenderScale(), &videos_);
  // The picture may be drawn bigger than it was rendered.
  const QPointF within = (at - frame.topLeft());
  const QPoint pixel(
      static_cast<int>(within.x() * plain.width() / frame.width()),
      static_cast<int>(within.y() * plain.height() / frame.height()));
  const bool is_inside = plain.rect().contains(pixel);
  return is_inside ? plain.pixelColor(pixel) : QColor();
}

const CompEffect* CompStage::Picked() const {
  assert(managers_.comp != nullptr);
  const ShotId shot = managers_.selection->shot();
  return managers_.comp->Find(shot, managers_.comp->PickedIn(shot));
}

QPointF CompStage::ArrowCenter() const { return FrameRect().center(); }

std::optional<QPointF> CompStage::HandleAt(Handle handle) {
  const CompEffect* effect = Picked();
  const bool has_effect = effect != nullptr;
  if (!has_effect) {
    return std::nullopt;
  }
  const ShotId shot = managers_.selection->shot();
  const Frame local = Local();
  if (handle == Handle::kArrow) {
    const bool has_arrow = ArrowKnob(*effect).has_value();
    if (!has_arrow) {
      return std::nullopt;
    }
    const Shot* found = FindShot(managers_.history->current(), shot);
    const double angle =
        found != nullptr ? LightAt(*found, *effect, local) : 0.0;
    const double radians = angle * std::numbers::pi / 180.0;
    return ArrowCenter() + QPointF(std::sin(radians), -std::cos(radians)) *
                               kArrowLength;
  }
  const bool is_b = handle == Handle::kB;
  const bool is_shown = is_b ? HasPointB(*effect) : HasPoints(*effect);
  if (!is_shown) {
    return std::nullopt;
  }
  const QTransform space =
      managers_.comp->PointSpace(shot, effect->id, local, Scale());
  const QPointF point = is_b ? PointBAt(*effect, local)
                             : PointAAt(*effect, local);
  return space.map(point) + FrameRect().topLeft();
}

CompStage::Handle CompStage::HandleUnder(QPointF point) {
  for (const Handle handle : {Handle::kA, Handle::kB, Handle::kArrow}) {
    const auto at = HandleAt(handle);
    const bool is_near =
        at.has_value() && QLineF(*at, point).length() <= kGrabRadius;
    if (is_near) {
      return handle;
    }
  }
  return Handle::kNone;
}

void CompStage::DragTo(QPointF point, bool is_snapped) {
  const CompEffect* effect = Picked();
  const bool has_effect = effect != nullptr;
  if (!has_effect) {
    return;
  }
  const ShotId shot = managers_.selection->shot();
  const Frame local = Local();
  if (dragging_ == Handle::kArrow) {
    double angle = AngleOf(ArrowCenter(), point);
    if (is_snapped) {
      angle = std::round(angle / 15.0) * 15.0;
    }
    const bool is_shared =
        effect->uses_shot_light && effect->kind == CompKind::kRimLight;
    Report(ProblemOf(
        is_shared ? managers_.comp->SetShotLight(shot, local, angle)
                  : managers_.comp->SetKnob(shot, effect->id,
                                            CompKnob::kAngle, local, angle)));
    return;
  }
  const QTransform space =
      managers_.comp->PointSpace(shot, effect->id, local, Scale());
  QPointF moved = space.inverted().map(point - FrameRect().topLeft());
  const bool is_b = dragging_ == Handle::kB;
  if (is_b && is_snapped) {
    // Shift keeps the line from A at 15 degree steps.
    const QPointF a = PointAAt(*effect, local);
    const QPointF way = moved - a;
    const double length = std::hypot(way.x(), way.y());
    const double step = std::numbers::pi / 12.0;
    const double turn = std::round(std::atan2(way.y(), way.x()) / step) * step;
    moved = a + QPointF(std::cos(turn), std::sin(turn)) * length;
  }
  Report(ProblemOf(
      managers_.comp->SetPoint(shot, effect->id, is_b, local, moved)));
}

void CompStage::PickUnder(QPointF point, bool is_adding) {
  const Here here = Locating(managers_);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, here.at.shot);
  const bool is_on_shot = here.at.IsValid() && shot != nullptr;
  if (!is_on_shot) {
    return;
  }
  const auto hit = HitTest(project, *shot, here.at.local,
                           point - FrameRect().topLeft(), Scale(),
                           renderer_.cache());
  if (!hit) {
    // A click on nothing picks the screen; Shift keeps the pick.
    const bool is_clearing = !is_adding;
    if (is_clearing) {
      managers_.selection->Clear();
    }
    return;
  }
  const bool is_new_layer = hit->layer != managers_.selection->layer();
  if (is_new_layer || !is_adding) {
    managers_.selection->SelectLayer(hit->layer);
  }
  const bool has_piece = !hit->piece.isEmpty();
  if (has_piece) {
    managers_.selection->SelectPieces({hit->piece}, is_adding);
  }
}

void CompStage::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0x24, 0x24, 0x27));
  const QRectF frame = FrameRect();
  const QImage picture = Picture();
  const bool has_picture = !picture.isNull();
  if (has_picture) {
    painter.drawImage(frame, picture);
  } else {
    painter.fillRect(frame, Qt::black);
  }
  painter.setRenderHint(QPainter::Antialiasing);
  PaintPicks(&painter);
  const auto arrow = HandleAt(Handle::kArrow);
  if (arrow) {
    painter.setPen(QPen(theme::kHandle, 2.0));
    painter.drawLine(ArrowCenter(), *arrow);
    const double angle = AngleOf(ArrowCenter(), *arrow) * std::numbers::pi /
                         180.0;
    const QPointF back(-std::sin(angle) * 12.0, std::cos(angle) * 12.0);
    const QPointF side(std::cos(angle) * 6.0, std::sin(angle) * 6.0);
    painter.setBrush(theme::kHandle);
    painter.drawPolygon(QPolygonF({*arrow, *arrow + back + side,
                                   *arrow + back - side}));
  }
  const auto a = HandleAt(Handle::kA);
  const auto b = HandleAt(Handle::kB);
  if (a && b) {
    painter.setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
    painter.drawLine(*a, *b);
  }
  painter.setPen(QPen(Qt::black, 1.0));
  painter.setBrush(theme::kHandle);
  for (const auto& point : {a, b}) {
    if (point) {
      painter.drawRect(QRectF(*point - QPointF(kHandleSize, kHandleSize) / 2,
                              QSizeF(kHandleSize, kHandleSize)));
    }
  }
  QString note = chip_;
  const bool is_off = managers_.comp->are_effects_off();
  const bool is_mask = managers_.comp->is_mask_shown();
  if (note.isEmpty() && is_off) {
    note = tr("Effects off (V)");
  }
  if (note.isEmpty() && is_mask) {
    note = tr("What it touches (K)");
  }
  const bool has_note = !note.isEmpty();
  if (has_note) {
    const QRectF box(frame.left() + 8, frame.top() + 8,
                     fontMetrics().horizontalAdvance(note) + 16, 22);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRect(box);
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, note);
  }
}

void CompStage::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  const CanvasSize& canvas = managers_.history->current().canvas;
  camera_.Wheel(*event, FitScale(), QSizeF(canvas.width, canvas.height),
                QSizeF(size()));
  ShowChip(QStringLiteral("%1%").arg(std::lround(Scale() * 100.0)));
  event->accept();
}

void CompStage::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_camera = camera_.Press(*event);
  if (is_camera) {
    return;
  }
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  const Handle handle = HandleUnder(event->position());
  const bool is_handle = handle != Handle::kNone;
  if (!is_handle) {
    PickUnder(event->position(),
              event->modifiers().testFlag(Qt::ShiftModifier));
    return;
  }
  const CompEffect* effect = Picked();
  assert(effect != nullptr);
  const QString label =
      handle == Handle::kArrow
          ? QStringLiteral("%1 %2").arg(
                CompManager::EffectName(*effect),
                CompManager::KnobLabel(*effect, CompKnob::kAngle)
                    .toLower())
          : tr("Move %1 point").arg(CompManager::EffectName(*effect));
  dragging_ = handle;
  drag_ = std::make_unique<EditScope>(managers_.history, label);
}

void CompStage::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_panning = camera_.Move(*event);
  if (is_panning) {
    update();
    return;
  }
  const bool is_dragging = dragging_ != Handle::kNone && drag_ != nullptr;
  if (!is_dragging) {
    const bool is_over = HandleUnder(event->position()) != Handle::kNone;
    setCursor(is_over ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return;
  }
  setCursor(Qt::ClosedHandCursor);
  DragTo(event->position(), event->modifiers().testFlag(Qt::ShiftModifier));
}

void CompStage::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool was_camera = camera_.Release(*event);
  if (was_camera) {
    return;
  }
  dragging_ = Handle::kNone;
  drag_.reset();
  setCursor(Qt::ArrowCursor);
}

void CompStage::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_cancel =
      event->key() == Qt::Key_Escape && drag_ != nullptr;
  if (is_cancel) {
    drag_->Cancel();
    drag_.reset();
    dragging_ = Handle::kNone;
    return;
  }
  // Shift+1 fits the whole frame, as on the other stages.
  const bool is_fit = event->modifiers().testFlag(Qt::ShiftModifier) &&
                      (event->key() == Qt::Key_1 ||
                       event->key() == Qt::Key_Exclam);
  if (is_fit) {
    camera_.Reset();
    update();
    return;
  }
  QWidget::keyPressEvent(event);
}

void CompStage::resizeEvent(QResizeEvent* event) {
  assert(event != nullptr);
  Forget();
  QWidget::resizeEvent(event);
}

}  // namespace snapper
