#include "ui/reel_preview.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <ranges>
#include <variant>
#include <vector>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "render/reel_text.h"
#include "ui/grab.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// How near a guide a dragged text catches, in screen pixels.
constexpr double kSnapPixels = 8.0;

std::vector<QPointF> CornersOf(const QRectF& box) {
  assert(box.width() >= 0.0);
  assert(box.height() >= 0.0);
  return {box.topLeft(), box.topRight(), box.bottomLeft(),
          box.bottomRight()};
}

}  // namespace

ReelPreview::ReelPreview(const Managers& managers)
    : managers_(managers), camera_(this), typing_(this) {
  assert(managers_.IsComplete());
  setMinimumSize(160, 90);
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  renderer_.SetDraftShown(managers_.draft != nullptr);  // drafts, never export
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(&typing_, &InlineTextEdit::Finished, this,
          [this](ClipId clip, const QString& text) {
            Report(ProblemOf(managers_.reel->SetText(clip, text)));
          });
  assert(minimumWidth() > 0);
}

double ReelPreview::FitScale() const {
  const Project& project = managers_.history->current();
  assert(IsValidCanvas(project.canvas));
  const double scale =
      std::min(static_cast<double>(width()) / project.canvas.width,
               static_cast<double>(height()) / project.canvas.height);
  return std::max(scale, 0.01);
}

double ReelPreview::Scale() const { return camera_.ScaleFor(FitScale()); }

QSizeF ReelPreview::FrameSize() const {
  const Project& project = managers_.history->current();
  const double scale = Scale();
  // As the frame is drawn: its pixel size.
  return QSizeF(std::max(1.0, std::round(project.canvas.width * scale)),
                std::max(1.0, std::round(project.canvas.height * scale)));
}

QPointF ReelPreview::Corner() const {
  const Project& project = managers_.history->current();
  assert(IsValidCanvas(project.canvas));
  const QPointF corner = camera_.CornerFor(
      Scale(), QSizeF(project.canvas.width, project.canvas.height),
      QSizeF(size()));
  return QPointF(std::floor(corner.x()), std::floor(corner.y()));
}

QRectF ReelPreview::ScreenRect(ClipId clip) const {
  assert(clip.value() >= 0);
  assert(managers_.playback != nullptr);
  const Project& project = managers_.history->current();
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  for (const ReelPiece& piece : ReelAt(project, now)) {
    const bool is_it = piece.clip->id == clip && IsTextClip(*piece.clip);
    if (is_it) {
      const QRectF box = ReelTextRect(project, *piece.clip);
      const double scale = Scale();
      return QRectF(Corner() + box.topLeft() * scale, box.size() * scale);
    }
  }
  return QRectF();
}

ClipId ReelPreview::TextAt(QPointF at) const {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(managers_.playback != nullptr);
  const Project& project = managers_.history->current();
  const auto pieces =
      ReelAt(project, managers_.playback->FrameOn(Timeline::kReel));
  // The top track shows on top, so it is hit first.
  for (const ReelPiece& piece : std::views::reverse(pieces)) {
    const bool is_hit = IsTextClip(*piece.clip) &&
                        ScreenRect(piece.clip->id).contains(at);
    if (is_hit) {
      return piece.clip->id;
    }
  }
  return ClipId();
}

ClipId ReelPreview::HandleAt(QPointF at) const {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(managers_.selection != nullptr);
  for (const ClipId clip : managers_.selection->clips()) {
    const QRectF box = ScreenRect(clip);
    const bool is_hit =
        !box.isEmpty() && NearestGrab(CornersOf(box), at) >= 0;
    if (is_hit) {
      return clip;
    }
  }
  return ClipId();
}

void ReelPreview::EditText(ClipId clip) {
  assert(clip.IsValid());
  assert(managers_.history != nullptr);
  const Clip* found = ClipOf(managers_.history->current().reel, clip);
  const bool is_text = found != nullptr && IsTextClip(*found);
  if (!is_text) {
    return;
  }
  QRectF box = ScreenRect(clip);
  const bool is_shown = !box.isEmpty();
  if (!is_shown) {
    // Not under the playhead: type in the middle of the frame.
    box = QRectF(width() / 4.0, height() / 3.0, width() / 2.0,
                 height() / 3.0);
  }
  typing_.Open(clip, std::get<TextSource>(found->source).text,
               box.toAlignedRect());
}

void ReelPreview::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  const bool is_empty = IsReelEmpty(project.reel);
  if (is_empty) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("Drag shots from the list, or a video file, onto "
                        "the tracks below. Ctrl+T adds text."));
    return;
  }
  // Never drawn finer than one frame pixel per pixel: zoomed in past
  // that the picture is magnified, not drawn again bigger.
  const double render_scale = std::min(Scale(), std::max(1.0, FitScale()));
  const QImage frame = renderer_.RenderReel(
      project, managers_.playback->FrameOn(Timeline::kReel), render_scale,
      &videos_);
  painter.drawImage(QRectF(Corner(), FrameSize()), frame);
  PaintGuides(&painter);
  PaintPicked(&painter);
}

void ReelPreview::PaintPicked(QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.selection != nullptr);
  for (const ClipId clip : managers_.selection->clips()) {
    const QRectF box = ScreenRect(clip);
    if (box.isEmpty()) {
      continue;
    }
    painter->setPen(QPen(theme::kPick, 1.5));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(box);
    painter->setPen(QPen(theme::kShadow, 1.0));
    painter->setBrush(theme::kHandle);
    for (const QPointF& corner : CornersOf(box)) {
      painter->drawRect(QRectF(corner.x() - kHandleDraw / 2.0,
                               corner.y() - kHandleDraw / 2.0, kHandleDraw,
                               kHandleDraw));
    }
  }
}

void ReelPreview::PaintGuides(QPainter* painter) const {
  assert(painter != nullptr);
  const bool is_dragging = drag_ != Drag::kNone && has_moved_;
  if (!is_dragging) {
    return;
  }
  const Project& project = managers_.history->current();
  const double scale = Scale();
  const QRectF frame(Corner(), QSizeF(project.canvas.width * scale,
                                      project.canvas.height * scale));
  // The title-safe margin while a text drags; the middle lines it caught.
  const QRectF safe = frame.adjusted(
      frame.width() * kTitleSafe, frame.height() * kTitleSafe,
      -frame.width() * kTitleSafe, -frame.height() * kTitleSafe);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(theme::kHandle, 1.0, Qt::DashLine));
  painter->drawRect(safe);
  if (is_snapped_x_) {
    painter->drawLine(QPointF(frame.center().x(), frame.top()),
                      QPointF(frame.center().x(), frame.bottom()));
  }
  if (is_snapped_y_) {
    painter->drawLine(QPointF(frame.left(), frame.center().y()),
                      QPointF(frame.right(), frame.center().y()));
  }
}

void ReelPreview::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  const Project& project = managers_.history->current();
  camera_.Wheel(*event, FitScale(),
                QSizeF(project.canvas.width, project.canvas.height),
                QSizeF(size()));
  update();
  event->accept();
}

void ReelPreview::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const bool is_camera = camera_.Press(*event);
  if (is_camera) {
    return;
  }
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  setFocus();
  const QPointF at = event->position();
  const ClipId handle = HandleAt(at);
  const ClipId text = handle.IsValid() ? handle : TextAt(at);
  if (!text.IsValid()) {
    return;
  }
  const bool is_shift = event->modifiers().testFlag(Qt::ShiftModifier);
  const bool is_picked = managers_.selection->clips().contains(text);
  if (!is_picked) {
    managers_.selection->PickClips(
        {text}, is_shift ? PickMode::kAdd : PickMode::kReplace);
  }
  const Clip* clip = ClipOf(managers_.history->current().reel, text);
  const ReelTextLook& look = LookOf(managers_.history->current().reel, *clip);
  drag_ = handle.IsValid() ? Drag::kScale : Drag::kMove;
  dragged_ = text;
  press_ = at;
  has_moved_ = false;
  start_place_ = look.place;
  start_size_ = look.letters.size;
  scope_ = std::make_unique<EditScope>(
      managers_.history,
      drag_ == Drag::kScale ? tr("Scale text") : tr("Move text"));
}

void ReelPreview::Follow(QPointF at, Qt::KeyboardModifiers modifiers) {
  assert(drag_ != Drag::kNone);
  assert(scope_ != nullptr);
  const bool is_still = !has_moved_ && !IsPastDeadZone(press_, at);
  if (is_still) {
    return;
  }
  has_moved_ = true;
  const Project& project = managers_.history->current();
  const double scale = Scale();
  const double width = project.canvas.width * scale;
  const double height = project.canvas.height * scale;
  QPointF place = start_place_;
  double size = start_size_;
  const bool is_move = drag_ == Drag::kMove;
  if (is_move) {
    const QPointF moved = at - press_;
    place += QPointF(moved.x() / width, moved.y() / height);
  } else {
    // Corners scale about the text's middle.
    const QPointF middle =
        Corner() + QPointF(start_place_.x() * width, start_place_.y() * height);
    const double from = std::hypot(press_.x() - middle.x(),
                                   press_.y() - middle.y());
    const double to = std::hypot(at.x() - middle.x(), at.y() - middle.y());
    size = start_size_ * to / std::max(from, 1.0);
  }
  const bool is_free = modifiers.testFlag(Qt::ControlModifier);
  const double reach = is_free ? 0.0 : kSnapPixels / std::max(width, 1.0);
  const auto placed = managers_.reel->PlaceText(dragged_, place, size, reach);
  if (!placed) {
    Report(placed.error().message);
    return;
  }
  is_snapped_x_ = placed->is_snapped_x;
  is_snapped_y_ = placed->is_snapped_y;
}

void ReelPreview::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_panning = camera_.Move(*event);
  if (is_panning) {
    update();
    return;
  }
  const QPointF at = event->position();
  const bool is_dragging = drag_ != Drag::kNone;
  if (is_dragging) {
    Follow(at, event->modifiers());
    return;
  }
  const bool is_handle = HandleAt(at).IsValid();
  const bool is_text = TextAt(at).IsValid();
  setCursor(is_handle ? Qt::SizeFDiagCursor
            : is_text ? Qt::SizeAllCursor
                      : Qt::ArrowCursor);
}

void ReelPreview::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool was_camera = camera_.Release(*event);
  if (was_camera) {
    return;
  }
  // Leaving the scope lands the drag as one step.
  scope_.reset();
  drag_ = Drag::kNone;
  dragged_ = ClipId();
  has_moved_ = false;
  is_snapped_x_ = false;
  is_snapped_y_ = false;
  update();
  assert(scope_ == nullptr);
}

void ReelPreview::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  // The first click opened a drag; a double click types instead.
  if (scope_ != nullptr) {
    scope_->Cancel();
  }
  scope_.reset();
  drag_ = Drag::kNone;
  const ClipId text = TextAt(event->position());
  if (text.IsValid()) {
    EditText(text);
  }
}

void ReelPreview::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  // Shift+1 fits the whole frame, as on the other stages.
  const bool is_fit = event->modifiers().testFlag(Qt::ShiftModifier) &&
                      (event->key() == Qt::Key_1 ||
                       event->key() == Qt::Key_Exclam);
  if (is_fit) {
    camera_.Reset();
    update();
    return;
  }
  const bool is_cancel =
      event->key() == Qt::Key_Escape && scope_ != nullptr;
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
  scope_->Cancel();
  scope_.reset();
  drag_ = Drag::kNone;
  has_moved_ = false;
  update();
}

}  // namespace snapper
