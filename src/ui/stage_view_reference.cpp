// StageView's references: drawn behind or over the layers on the Pose
// tab stage only, with a thin dashed border; the picked one moves and
// scales with the mouse, one undo step a drag.

#include <QLineF>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "edit/history_manager.h"
#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "render/reference_paint.h"
#include "ui/grab.h"
#include "ui/problem.h"
#include "ui/stage_view.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Every shown reference's edge: thin and faint, so it reads as a guide.
constexpr QColor kReferenceEdge{255, 255, 255, 90};

QSize CanvasOf(const Project& project) {
  assert(IsValidCanvas(project.canvas));
  return QSize(project.canvas.width, project.canvas.height);
}

// The corners of box, where the scale handles sit.
std::vector<QPointF> CornersOf(const QRectF& box) {
  assert(box.width() >= 0.0 && box.height() >= 0.0);
  return {box.topLeft(), box.topRight(), box.bottomLeft(),
          box.bottomRight()};
}

}  // namespace

QImage StageView::ReferencesUnder(const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_references = managers_.references != nullptr &&
                              shot != nullptr && !shot->references.empty();
  if (!has_references) {
    return QImage();
  }
  const QSize canvas = CanvasOf(project);
  const QSize size(
      std::max(1, static_cast<int>(std::lround(canvas.width() * frame.scale))),
      std::max(1,
               static_cast<int>(std::lround(canvas.height() * frame.scale))));
  QImage under;
  // The first row is on top: drawn last.
  for (auto it = shot->references.rbegin(); it != shot->references.rend();
       ++it) {
    const Reference& reference = *it;
    const QImage picture =
        reference.is_in_front
            ? QImage()
            : managers_.references->Picture(reference, frame.local);
    const bool is_drawn = !picture.isNull();
    if (!is_drawn) {
      continue;
    }
    const bool is_first = under.isNull();
    if (is_first) {
      under = QImage(size, QImage::Format_ARGB32_Premultiplied);
      under.fill(Qt::transparent);
    }
    QPainter painter(&under);
    PaintReference(reference, picture, canvas, frame.scale,
                   IsReferenceEnded(reference, frame.local), &painter);
  }
  return under;
}

void StageView::PaintReferencesOver(const StageFrame& frame, QSize shown,
                                    QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool has_references = managers_.references != nullptr &&
                              shot != nullptr && !shot->references.empty();
  if (!has_references) {
    return;
  }
  const QSize canvas = CanvasOf(project);
  painter->save();
  painter->translate(frame.corner);
  painter->setClipRect(QRectF(QPointF(), QSizeF(shown)));
  bool is_any_ended = false;
  for (auto it = shot->references.rbegin(); it != shot->references.rend();
       ++it) {
    const Reference& reference = *it;
    const bool is_visible = managers_.references->IsVisible(reference) &&
                            !(frame.local < reference.start);
    if (!is_visible) {
      continue;
    }
    const bool is_ended = IsReferenceEnded(reference, frame.local);
    is_any_ended = is_any_ended || is_ended;
    if (reference.is_in_front) {
      PaintReference(reference,
                     managers_.references->Picture(reference, frame.local),
                     canvas, frame.scale, is_ended, painter);
    }
    const QRectF place = ReferenceRect(reference, canvas);
    painter->setPen(QPen(kReferenceEdge, 1.0, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(
        QRectF(place.topLeft() * frame.scale, place.size() * frame.scale));
  }
  painter->restore();
  const QRectF box = PickedReferenceBox(frame);
  const bool has_pick = !box.isEmpty();
  if (has_pick) {
    painter->setPen(QPen(theme::kPick, 1.5, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(box);
    painter->setPen(QPen(theme::kShadow, 1.0));
    painter->setBrush(theme::kHandle);
    for (const QPointF corner : CornersOf(box)) {
      painter->drawRect(QRectF(corner - QPointF(kHandleDraw / 2, kHandleDraw / 2),
                               QSizeF(kHandleDraw, kHandleDraw)));
    }
  }
  if (is_any_ended) {
    const QString words = tr("reference ended");
    const QRectF chip(frame.corner + QPointF(8.0, 8.0),
                      QSizeF(painter->fontMetrics().horizontalAdvance(words) +
                                 12.0,
                             painter->fontMetrics().height() + 6.0));
    painter->setPen(Qt::NoPen);
    painter->setBrush(theme::kFaceDark);
    painter->drawRoundedRect(chip, 4.0, 4.0);
    painter->setPen(theme::kText);
    painter->drawText(chip, Qt::AlignCenter, words);
  }
}

QRectF StageView::PickedReferenceBox(const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.history != nullptr);
  const bool has_manager = managers_.references != nullptr;
  if (!has_manager) {
    return QRectF();
  }
  const ReferenceId picked = managers_.references->PickedIn(frame.shot);
  const Reference* reference =
      picked.IsValid() ? managers_.references->Find(frame.shot, picked)
                       : nullptr;
  if (reference == nullptr) {
    return QRectF();
  }
  const QRectF place =
      ReferenceRect(*reference, CanvasOf(managers_.history->current()));
  return QRectF(frame.corner + place.topLeft() * frame.scale,
                place.size() * frame.scale);
}

bool StageView::ReferencePress(QPointF point, const StageFrame& frame) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(reference_scope_ == nullptr);
  const QRectF box = PickedReferenceBox(frame);
  const bool has_pick = !box.isEmpty();
  if (!has_pick) {
    return false;
  }
  const int corner = NearestGrab(CornersOf(box), point, kGrabReach);
  const bool is_scale = corner >= 0;
  const bool is_inside = box.contains(point);
  const ReferenceId picked = managers_.references->PickedIn(frame.shot);
  if (!is_scale && !is_inside) {
    // A click elsewhere goes on to the dolls and lets the reference go.
    managers_.references->Pick(frame.shot, ReferenceId());
    return false;
  }
  const Reference* reference = managers_.references->Find(frame.shot, picked);
  assert(reference != nullptr);
  reference_drag_ =
      ReferenceDrag{frame.shot,        picked,           is_scale,
                    point,             box.center(),     reference->center,
                    reference->scale,  frame.scale};
  reference_scope_ = std::make_unique<EditScope>(
      managers_.history,
      is_scale ? tr("Scale reference") : tr("Move reference"));
  setCursor(is_scale ? Qt::SizeFDiagCursor : Qt::ClosedHandCursor);
  return true;
}

bool StageView::ReferenceMove(QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const bool is_dragging = reference_drag_.has_value();
  if (!is_dragging) {
    return false;
  }
  const ReferenceDrag& drag = *reference_drag_;
  assert(drag.frame_scale > 0.0);
  QPointF center = drag.center;
  double scale = drag.scale;
  if (drag.is_scale) {
    const double from = QLineF(drag.middle, drag.press).length();
    const double to = QLineF(drag.middle, point).length();
    const bool is_measurable = from > 1.0;
    scale = is_measurable ? drag.scale * to / from : drag.scale;
  } else {
    center = drag.center + (point - drag.press) / drag.frame_scale;
  }
  emit Problem(ProblemOf(
      managers_.references->SetPlace(drag.shot, drag.id, center, scale)));
  update();
  return true;
}

bool StageView::ReferenceRelease() {
  const bool is_dragging = reference_drag_.has_value();
  if (!is_dragging) {
    return false;
  }
  reference_scope_.reset();
  reference_drag_.reset();
  unsetCursor();
  update();
  assert(reference_scope_ == nullptr);
  return true;
}

bool StageView::CancelReferenceDrag() {
  const bool is_dragging = reference_drag_.has_value();
  if (!is_dragging) {
    return false;
  }
  assert(reference_scope_ != nullptr);
  reference_scope_->Cancel();
  reference_scope_.reset();
  reference_drag_.reset();
  unsetCursor();
  update();
  return true;
}

void StageView::DropReferences(const QStringList& files) {
  assert(!files.isEmpty());
  assert(managers_.references != nullptr);
  const auto frame = CurrentFrame();
  const ShotId shot =
      frame.has_value() ? frame->shot : managers_.selection->shot();
  QStringList problems;
  for (const QString& file : files) {
    const auto added = managers_.references->Add(shot, file);
    if (!added) {
      problems.append(added.error().message);
    }
  }
  emit Problem(problems.join(QStringLiteral(" ")));
}

}  // namespace snapper
