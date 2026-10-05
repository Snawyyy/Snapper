#include "ui/shot_strip.h"

#include <QColorDialog>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kPixelsPerFrame = 2.0;
constexpr double kMinBlock = 56.0;
constexpr double kAddWidth = 28.0;
constexpr double kEdgeReach = 5.0;
constexpr int kStripHeight = 40;
constexpr std::array<int, 4> kTransitionLengths = {2, 4, 6, 8};

double WidthOf(const Shot& shot) {
  assert(shot.length.index() >= 1);
  const double width = shot.length.index() * kPixelsPerFrame;
  assert(width > 0.0);
  return std::max(width, kMinBlock);
}

}  // namespace

ShotStrip::ShotStrip(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  setFixedHeight(kStripHeight);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  const auto update_me = [this] { update(); };
  connect(managers_.history, &HistoryManager::Changed, this, update_me);
  connect(managers_.selection, &SelectionManager::Changed, this, update_me);
  assert(hasMouseTracking());
}

std::vector<ShotStrip::Block> ShotStrip::Blocks() const {
  assert(managers_.history != nullptr);
  std::vector<Block> blocks;
  double x = 2.0;
  for (const auto& shot : managers_.history->current().shots) {
    const double width = WidthOf(*shot);
    blocks.push_back({shot->id, QRectF(x, 2.0, width, kStripHeight - 4.0)});
    x += width + 2.0;
  }
  assert(blocks.size() <= static_cast<size_t>(kMaxShots));
  return blocks;
}

QRectF ShotStrip::AddBlock() const {
  const auto blocks = Blocks();
  const double x = blocks.empty() ? 2.0 : blocks.back().rect.right() + 2.0;
  assert(x >= 0.0);
  return QRectF(x, 2.0, kAddWidth, kStripHeight - 4.0);
}

void ShotStrip::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  const Project& project = managers_.history->current();
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  for (const Block& block : Blocks()) {
    const Shot& shot = *FindShot(project, block.shot);
    const bool is_picked = block.shot == managers_.selection->shot();
    painter.fillRect(block.rect, theme::kFace);
    painter.fillRect(
        QRectF(block.rect.topLeft(), QSizeF(6, block.rect.height())),
        shot.background);
    painter.setPen(QPen(is_picked ? theme::kPick : theme::kShadow,
                        is_picked ? 2.0 : 1.0));
    painter.drawRect(block.rect);
    painter.setPen(theme::kText);
    painter.drawText(block.rect.adjusted(10, 2, -4, -2),
                     Qt::AlignLeft | Qt::AlignTop, shot.name);
    painter.setPen(theme::kTextOff);
    const bool has_transition = shot.transition.kind != TransitionKind::kCut;
    painter.drawText(
        block.rect.adjusted(10, 2, -4, -2), Qt::AlignLeft | Qt::AlignBottom,
        tr("%1 f%2").arg(shot.length.index())
            .arg(has_transition
                     ? QStringLiteral(" > ") +
                           TransitionName(shot.transition.kind)
                     : QString()));
  }
  const QRectF add = AddBlock();
  painter.fillRect(add, theme::kFace);
  painter.setPen(theme::kText);
  painter.drawText(add, Qt::AlignCenter, QStringLiteral("+"));
}

void ShotStrip::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QPointF at = event->position();
  const bool is_left = event->button() == Qt::LeftButton;
  const bool is_add = is_left && AddBlock().contains(at);
  if (is_add) {
    const int after = ShotIndex(managers_.history->current(),
                                managers_.selection->shot());
    const auto added = managers_.shots->Add(after < 0 ? -1 : after + 1);
    if (added) {
      Pick(*added);
    }
    Report(ProblemOf(added));
    return;
  }
  for (const Block& block : Blocks()) {
    const bool is_hit = is_left && block.rect.contains(at);
    if (!is_hit) {
      continue;
    }
    Pick(block.shot);
    const bool is_edge = block.rect.right() - at.x() <= kEdgeReach;
    drag_ = is_edge ? Drag::kLength : Drag::kMove;
    dragged_ = block.shot;
    drag_start_x_ = at.x();
    drag_start_length_ = FindShot(managers_.history->current(),
                                  block.shot)->length;
    scope_ = is_edge ? std::make_unique<EditScope>(managers_.history,
                                                    tr("Change shot length"))
                     : nullptr;
    return;
  }
}

void ShotStrip::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QPointF at = event->position();
  bool is_on_edge = false;
  for (const Block& block : Blocks()) {
    is_on_edge = is_on_edge || (block.rect.contains(at) &&
                                block.rect.right() - at.x() <= kEdgeReach);
  }
  setCursor(is_on_edge || drag_ == Drag::kLength ? Qt::SizeHorCursor
                                                 : Qt::ArrowCursor);
  const bool is_resizing = drag_ == Drag::kLength;
  if (is_resizing) {
    const int frames = static_cast<int>(
        std::lround((at.x() - drag_start_x_) / kPixelsPerFrame));
    const int length = std::max(1, drag_start_length_.index() + frames);
    Report(ProblemOf(managers_.shots->SetLength(dragged_, Frame(length))));
  }
}

void ShotStrip::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_moving = drag_ == Drag::kMove &&
                         std::abs(event->position().x() - drag_start_x_) > 4;
  if (is_moving) {
    int index = 0;
    for (const Block& block : Blocks()) {
      const bool is_past = block.rect.center().x() < event->position().x() &&
                           block.shot != dragged_;
      index += is_past ? 1 : 0;
    }
    Report(ProblemOf(managers_.shots->Move(dragged_, index)));
  }
  scope_.reset();
  drag_ = Drag::kNone;
}

void ShotStrip::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  for (const Block& block : Blocks()) {
    const bool is_hit = block.rect.contains(event->pos());
    if (is_hit) {
      Pick(block.shot);
      Menu(block.shot, event->globalPos());
      return;
    }
  }
}

void ShotStrip::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const ShotId shot = managers_.selection->shot();
  const bool is_delete = shot.IsValid() &&
                         (event->key() == Qt::Key_Delete ||
                          event->key() == Qt::Key_Backspace);
  if (is_delete) {
    Report(ProblemOf(managers_.shots->Remove(shot)));
    return;
  }
  QWidget::keyPressEvent(event);
}

void ShotStrip::Pick(ShotId shot) {
  assert(shot.IsValid());
  const Project& project = managers_.history->current();
  const int index = ShotIndex(project, shot);
  managers_.selection->SelectShot(shot);
  const bool is_found = index >= 0;
  if (is_found) {
    managers_.playback->Seek(ShotStart(project, index));
  }
}

void ShotStrip::Menu(ShotId shot, QPoint where) {
  assert(shot.IsValid());
  const Shot* found = FindShot(managers_.history->current(), shot);
  assert(found != nullptr);
  QMenu menu(this);
  connect(menu.addAction(tr("Rename...")), &QAction::triggered, this,
          [this, shot, name = found->name] {
            bool is_ok = false;
            const QString text = QInputDialog::getText(
                this, tr("Rename shot"), tr("Name"), QLineEdit::Normal, name,
                &is_ok);
            if (is_ok) {
              Report(ProblemOf(managers_.shots->Rename(shot, text)));
            }
          });
  connect(menu.addAction(tr("Background colour...")), &QAction::triggered,
          this, [this, shot, colour = found->background] {
            const QColor picked = QColorDialog::getColor(colour, this);
            const bool is_picked = picked.isValid();
            if (is_picked) {
              Report(ProblemOf(managers_.shots->SetBackground(shot, picked)));
            }
          });
  QMenu& into = *menu.addMenu(tr("Transition into next"));
  for (int kind = 0; kind < kTransitionKindCount; ++kind) {
    const auto chosen = static_cast<TransitionKind>(kind);
    QMenu& lengths = *into.addMenu(TransitionName(chosen));
    for (const int frames : kTransitionLengths) {
      connect(lengths.addAction(tr("%1 frames").arg(frames)),
              &QAction::triggered, this, [this, shot, chosen, frames] {
                Report(ProblemOf(managers_.shots->SetTransition(
                    shot, {chosen, Frame(frames)})));
              });
    }
  }
  connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this,
          [this, shot] {
            const auto copy = managers_.shots->Duplicate(shot);
            Report(ProblemOf(copy));
          });
  connect(menu.addAction(tr("Delete")), &QAction::triggered, this,
          [this, shot] { Report(ProblemOf(managers_.shots->Remove(shot))); });
  menu.exec(where);
}


}  // namespace snapper
