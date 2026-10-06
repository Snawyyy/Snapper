#include "ui/stage_view.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/project_edits.h"
#include "render/stage_geometry.h"
#include "render/stage_hit.h"
#include "ui/playhead.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Room left around the frame so handles at its edge stay grabbable.
constexpr double kMargin = 0.94;
constexpr double kHandleRadius = 4.0;
constexpr double kBarThin = 6.0;
// Warp grids: faint lines, small dots (they grab big).
constexpr QColor kWarpLine{255, 255, 255, 70};
constexpr double kWarpDot = 2.5;
constexpr double kBarLong = 14.0;
constexpr int kWheelUnit = 120;

}  // namespace

StageView::StageView(const Managers& managers)
    : managers_(managers), tool_(managers, renderer_.cache()) {
  assert(managers_.IsComplete());
  setFocusPolicy(Qt::StrongFocus);
  setMinimumSize(320, 180);
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          update_me);
  assert(focusPolicy() == Qt::StrongFocus);
}

std::optional<StageFrame> StageView::CurrentFrame() const {
  const Project& project = managers_.history->current();
  const auto spot = SpotOf(managers_);
  assert(IsValidCanvas(project.canvas));
  if (!spot) {
    return std::nullopt;
  }
  const double scale =
      kMargin * std::min(width() / double(project.canvas.width),
                         height() / double(project.canvas.height));
  const QPointF corner((width() - project.canvas.width * scale) / 2.0,
                       (height() - project.canvas.height * scale) / 2.0);
  assert(scale > 0.0);
  return StageFrame{spot->shot, spot->local, scale, corner};
}

void StageView::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  const auto frame = CurrentFrame();
  if (!frame) {
    painter.setPen(theme::kTextOff);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("No shot here. Add one in the shot strip above."));
    return;
  }
  const QImage image = renderer_.RenderFrame(
      managers_.history->current(), managers_.playback->frame(),
      frame->scale);
  painter.drawImage(frame->corner, image);
  painter.setRenderHint(QPainter::Antialiasing);
  PaintHandles(*frame, &painter);
  const QRectF box = tool_.Box();
  const bool has_box = !box.isEmpty();
  if (has_box) {
    painter.resetTransform();
    painter.setPen(QPen(theme::kPick, 1.0, Qt::DashLine));
    painter.setBrush(QColor(theme::kPick.red(), theme::kPick.green(),
                            theme::kPick.blue(), 40));
    painter.drawRect(box);
  }
}

void StageView::PaintHandles(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const Project& project = managers_.history->current();
  const SelectionManager& selection = *managers_.selection;
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_picked = shot != nullptr && selection.shot() == frame.shot &&
                         !selection.picks().empty();
  if (!is_picked) {
    return;
  }
  painter->translate(frame.corner);
  for (const Pick& pick : selection.picks()) {
    const bool is_whole = pick.piece.isEmpty();
    painter->setPen(QPen(theme::kPick, 2.0));
    painter->setBrush(Qt::NoBrush);
    if (is_whole) {
      painter->drawPolygon(LayerShape(project, *shot, pick.layer, frame.local,
                                      frame.scale, renderer_.cache()));
      const auto turn = TurnHandlesOf(project, *shot, pick.layer,
                                      frame.local, frame.scale,
                                      renderer_.cache());
      if (turn) {
        // Long the way they drag: lean up and down, swivel sideways.
        painter->setPen(QPen(theme::kShadow, 1.0));
        painter->setBrush(theme::kHandle);
        painter->drawRect(QRectF(turn->lean - QPointF(kBarThin / 2,
                                                      kBarLong / 2),
                                 QSizeF(kBarThin, kBarLong)));
        painter->drawRect(QRectF(turn->swivel - QPointF(kBarLong / 2,
                                                        kBarThin / 2),
                                 QSizeF(kBarLong, kBarThin)));
      }
      continue;
    }
    const auto outline = PieceOnScreen(project, *shot, pick.layer,
                                       pick.piece, frame.local, frame.scale);
    PaintWarp(frame, pick, painter);
    painter->setPen(QPen(theme::kPick, 2.0));
    painter->setBrush(Qt::NoBrush);
    if (outline) {
      painter->drawPolygon(outline->outline);
      painter->setPen(QPen(theme::kShadow, 1.0));
      painter->setBrush(theme::kHandle);
      painter->drawEllipse(outline->pivot, kHandleRadius, kHandleRadius);
    }
  }
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  for (const IkHandle& handle : IkHandles(project, *shot, selection.layer(),
                                          frame.local, frame.scale)) {
    painter->drawRect(QRectF(handle.point - QPointF(kHandleRadius,
                                                    kHandleRadius),
                             QSizeF(kHandleRadius * 2, kHandleRadius * 2)));
  }
}

void StageView::PaintWarp(const StageFrame& frame, const Pick& pick,
                          QPainter* painter) {
  assert(painter != nullptr);
  assert(!pick.piece.isEmpty());
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const auto warp =
      shot != nullptr ? PieceWarpOnScreen(project, *shot, pick.layer,
                                          pick.piece, frame.local,
                                          frame.scale)
                      : std::nullopt;
  if (!warp) {
    return;
  }
  const int across = warp->grid.columns + 1;
  const auto at = [&warp, across](int row, int column) {
    return warp->to_screen.map(
        warp->points[static_cast<size_t>(row * across + column)]);
  };
  // A faint grid, so the dots read as one sheet.
  painter->setPen(QPen(kWarpLine, 1.0));
  painter->setBrush(Qt::NoBrush);
  for (int row = 0; row <= warp->grid.rows; ++row) {
    for (int column = 0; column <= warp->grid.columns; ++column) {
      const bool has_right = column < warp->grid.columns;
      if (has_right) {
        painter->drawLine(at(row, column), at(row, column + 1));
      }
      const bool has_below = row < warp->grid.rows;
      if (has_below) {
        painter->drawLine(at(row, column), at(row + 1, column));
      }
    }
  }
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  for (const QPointF& point : warp->points) {
    painter->drawEllipse(warp->to_screen.map(point), kWarpDot, kWarpDot);
  }
  const auto dot = tool_.PickedDot(frame);
  const bool is_dot_here =
      dot.has_value() && dot->layer == pick.layer && dot->piece == pick.piece;
  if (is_dot_here) {
    PaintReach(*warp, dot->point, painter);
  }
}

void StageView::PaintReach(const WarpOnScreen& warp, int point,
                           QPainter* painter) {
  assert(painter != nullptr);
  assert(point >= 0 && point < warp.grid.PointCount());
  const QPointF spot = warp.points[static_cast<size_t>(point)];
  const double reach = warp.reach[static_cast<size_t>(point)];
  const QPointF on_screen = warp.to_screen.map(spot);
  // Picked is orange; the faint ring is how far the rubber reaches, in
  // the drawing's own grid cells.
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(on_screen, kWarpDot + 2, kWarpDot + 2);
  const bool has_reach = reach > 0.0;
  if (has_reach) {
    painter->save();
    painter->setTransform(warp.to_screen, true);
    QColor faint = theme::kPick;
    faint.setAlpha(150);
    painter->setPen(QPen(faint, 0.0, Qt::DashLine));
    painter->drawEllipse(spot,
                         reach * warp.size.width() / warp.grid.columns,
                         reach * warp.size.height() / warp.grid.rows);
    painter->restore();
  }
  painter->setPen(theme::kText);
  painter->drawText(on_screen + QPointF(kWarpDot + 4, -kWarpDot - 4),
                    tr("reach %1").arg(reach, 0, 'f', 1));
}

void StageView::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const auto frame = CurrentFrame();
  const bool is_usable = frame.has_value() &&
                         event->button() == Qt::LeftButton;
  if (is_usable) {
    tool_.Press(event->position(),
                event->modifiers().testFlag(Qt::ShiftModifier),
                event->modifiers().testFlag(Qt::ControlModifier), *frame);
    ReportTool();
  }
  assert(!tool_.IsDragging() || is_usable);
}

void StageView::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_dragging = tool_.IsDragging();
  if (is_dragging) {
    tool_.Move(event->position(),
               event->modifiers().testFlag(Qt::ShiftModifier));
    ReportTool();
    update();
  }
  assert(event->type() == QEvent::MouseMove);
}

void StageView::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_left = event->button() == Qt::LeftButton;
  if (is_left) {
    tool_.Release();
    update();
  }
  assert(!is_left || !tool_.IsDragging());
}

void StageView::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const auto frame = CurrentFrame();
  const bool is_usable = frame.has_value() &&
                         event->button() == Qt::LeftButton;
  if (is_usable) {
    tool_.PickWhole(event->position(), *frame);
    update();
  }
  assert(!tool_.IsDragging());
}

void StageView::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  // Some systems turn Shift+wheel into a sideways scroll.
  const QPoint turn = event->angleDelta();
  // Touchpads send small steps; they add up to whole notches.
  wheel_left_ += turn.y() != 0 ? turn.y() : turn.x();
  const int notches = wheel_left_ / kWheelUnit;
  wheel_left_ -= notches * kWheelUnit;
  const auto frame = CurrentFrame();
  const bool is_usable = notches != 0 && frame.has_value() &&
                         !tool_.IsDragging();
  if (is_usable) {
    tool_.Wheel(notches, event->modifiers().testFlag(Qt::ShiftModifier),
                *frame);
    ReportTool();
  }
  event->accept();
  assert(event->isAccepted());
}

void StageView::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (is_escape) {
    // Escape backs out one thing at a time: the drag, the picked warp
    // dot, then the pick.
    const bool was_dragging = tool_.IsDragging();
    tool_.Cancel();
    const bool had_dot = !was_dragging && tool_.DropDot();
    const bool is_clearing = !was_dragging && !had_dot;
    if (is_clearing) {
      managers_.selection->Clear();
    }
    update();
    event->accept();
    return;
  }
  if (is_all) {
    PickAll();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
  assert(!is_escape);
}

void StageView::PickAll() {
  const auto frame = CurrentFrame();
  assert(managers_.selection != nullptr);
  const Shot* shot =
      frame ? FindShot(managers_.history->current(), frame->shot) : nullptr;
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return;
  }
  // Inside a picked doll: all its pieces; otherwise every layer.
  const LayerId focus = managers_.selection->layer();
  const Doll* doll = focus.IsValid()
                         ? DollOfLayer(managers_.history->current(),
                                       frame->shot, focus)
                         : nullptr;
  std::set<Pick> all;
  const bool is_in_doll = doll != nullptr;
  if (is_in_doll) {
    for (const RigPiece& piece : doll->rig.pieces) {
      all.insert({focus, piece.name});
    }
  } else {
    for (const Layer& layer : shot->layers) {
      const bool is_pickable =
          !std::holds_alternative<EffectLayer>(layer.content);
      if (is_pickable) {
        all.insert({layer.id, QString()});
      }
    }
  }
  managers_.selection->SelectShot(frame->shot);
  managers_.selection->PickThings(all, PickMode::kReplace, focus);
}

void StageView::ReportTool() {
  assert(managers_.history != nullptr);
  assert(tool_.problem().size() < 100000);
  emit Problem(tool_.problem());
}

}  // namespace snapper
