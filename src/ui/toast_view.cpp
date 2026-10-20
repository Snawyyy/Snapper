#include "ui/toast_view.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <memory>

#include "ui/motion.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

constexpr int kToastWidth = 420;
constexpr int kSideGutter = 16;
constexpr int kBottomGap = 16;
constexpr int kBarHeight = 2;
// A sticky note's corner curls up this far.
constexpr int kPeel = 14;
constexpr QColor kNoteInk{0x3a, 0x2f, 0x1e};

// The note's paper for each kind, in ToastKind order.
QColor PaperOf(ToastKind kind) {
  switch (kind) {
    case ToastKind::kProblem:
      return {0xf2, 0x9a, 0x9a};
    case ToastKind::kCheer:
      return {0xb6, 0xe0, 0x9e};
    case ToastKind::kAchievement:
      return {0xf2, 0xc2, 0x30};
    case ToastKind::kInfo:
      break;
  }
  return {0xf6, 0xd8, 0x60};
}

}  // namespace

ToastItem::ToastItem(const Toast& toast, QWidget* parent)
    : QFrame(parent),
      id_(toast.id),
      left_ms_(toast.left_ms),
      layout_(this),
      label_(toast.text),
      undo_(tr("Undo")) {
  assert(toast.id > 0);
  assert(parent != nullptr);
  setFocusPolicy(Qt::NoFocus);
  undo_.setFocusPolicy(Qt::NoFocus);
  label_.setWordWrap(true);
  layout_.setContentsMargins(10, 8, 10, 8);
  layout_.setSpacing(10);
  layout_.addWidget(&label_, 1);
  layout_.addWidget(&undo_);
  connect(&undo_, &QPushButton::clicked, this,
          [this] { emit UndoPressed(id_); });
  Update(toast);
}

void ToastItem::Update(const Toast& toast) {
  assert(toast.id == id_);
  assert(toast.left_ms <= kToastMs);
  left_ms_ = toast.left_ms;
  kind_ = toast.kind;
  QPalette ink = label_.palette();
  ink.setColor(QPalette::WindowText, kNoteInk);
  label_.setPalette(ink);
  undo_.setVisible(toast.has_undo);
  undo_.setToolTip(tr("Undo %1").arg(toast.undo_label));
  update();
}

void ToastItem::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(id_ > 0);
  emit Clicked(id_);
  event->accept();
}

void ToastItem::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(left_ms_ <= kToastMs);
  // A sticky note on the desk's edge: paper of its kind, a darker band
  // where the glue is, and its bottom right corner curling up as its
  // time runs out.
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QColor paper = PaperOf(kind_);
  const int peel = kPeel * (kToastMs - std::max(left_ms_, 0)) / kToastMs + 4;
  QPolygon sheet;
  sheet << QPoint(0, 0) << QPoint(width(), 0)
        << QPoint(width(), height() - peel) << QPoint(width() - peel, height())
        << QPoint(0, height());
  painter.setPen(Qt::NoPen);
  painter.setBrush(paper);
  painter.drawPolygon(sheet);
  painter.fillRect(QRect(0, 0, width(), 5), paper.darker(110));
  painter.setBrush(paper.darker(125));
  painter.drawPolygon(QPolygon{QPoint(width(), height() - peel),
                               QPoint(width() - peel, height()),
                               QPoint(width() - peel, height() - peel)});
  const int bar = (width() - peel) * std::max(left_ms_, 0) / kToastMs;
  painter.fillRect(0, height() - kBarHeight, bar, kBarHeight,
                   paper.darker(160));
}

ToastView::ToastView(ToastManager* toasts, MotionManager* motion,
                     QWidget* host, QWidget* anchor)
    : QWidget(host),
      toasts_(toasts),
      motion_(motion),
      host_(host),
      anchor_(anchor),
      layout_(this) {
  assert(toasts_ != nullptr && motion_ != nullptr);
  assert(host_ != nullptr && anchor_ != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(4);
  setFocusPolicy(Qt::NoFocus);
  hide();
  anchor_->installEventFilter(this);
  qApp->installEventFilter(this);
  connect(toasts_, &ToastManager::Changed, this, &ToastView::Sync);
}

bool ToastView::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched != nullptr);
  const QEvent::Type type = event->type();
  const bool is_anchor_changed =
      watched == anchor_ && (type == QEvent::Resize || type == QEvent::Move ||
                             type == QEvent::Show);
  if (is_anchor_changed) {
    Place();
    return false;
  }
  const bool is_key = type == QEvent::KeyPress;
  const bool is_escape =
      is_key && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape;
  const bool is_for_us = is_escape && host_->isActiveWindow();
  // Escape also does whatever it did before; the toast just goes too.
  if (is_for_us) {
    toasts_->DismissNewest();
  }
  return false;
}

void ToastView::Fade(ToastItem* item, double from, double to,
                     bool is_leaving) {
  assert(item != nullptr);
  assert(from >= 0.0 && to <= 1.0);
  auto effect = std::make_unique<QGraphicsOpacityEffect>(item);
  QGraphicsOpacityEffect* shown = effect.release();
  item->setGraphicsEffect(shown);
  const MotionKind kind = is_leaving ? MotionKind::kPop : MotionKind::kMove;
  // A newer fade replaces the effect and deletes this one while this
  // animation may still run, so it holds the effect weakly.
  const QPointer<QGraphicsOpacityEffect> weak(shown);
  QVariantAnimation* animation = motion_->Animate(
      item, kind, from, to, [weak](const QVariant& v) {
        if (weak != nullptr) {
          weak->setOpacity(v.toDouble());
        }
      });
  const bool is_instant = animation == nullptr;
  const bool is_gone_now = is_instant && is_leaving;
  const bool is_gone_later = !is_instant && is_leaving;
  if (is_gone_now) {
    item->deleteLater();
  }
  if (is_gone_later) {
    connect(animation, &QVariantAnimation::finished, item,
            &QObject::deleteLater);
  }
}

void ToastView::Sync() {
  assert(toasts_ != nullptr);
  assert(motion_ != nullptr);
  const ToastQueue& queue = toasts_->queue();
  std::vector<QPointer<ToastItem>> kept;
  for (const QPointer<ToastItem>& item : items_) {
    const Toast* toast = item ? queue.Find(item->id()) : nullptr;
    const bool is_still_up = toast != nullptr;
    if (is_still_up) {
      item->Update(*toast);
      kept.push_back(item);
    } else if (item) {
      // Fading out: it must not catch a click meant for what is behind.
      item->setAttribute(Qt::WA_TransparentForMouseEvents);
      Fade(item, 1.0, 0.0, true);
    }
  }
  items_ = kept;
  for (const Toast& toast : queue.toasts()) {
    const auto has_item = [&toast](const QPointer<ToastItem>& item) {
      return item->id() == toast.id;
    };
    const bool is_shown =
        std::any_of(items_.begin(), items_.end(), has_item);
    if (!is_shown) {
      AddItem(toast);
    }
  }
  Place();
}

void ToastView::AddItem(const Toast& toast) {
  assert(toast.id > 0);
  assert(toasts_ != nullptr);
  auto owned = std::make_unique<ToastItem>(toast, this);
  ToastItem* item = owned.release();
  connect(item, &ToastItem::Clicked, toasts_, &ToastManager::Dismiss);
  connect(item, &ToastItem::UndoPressed, toasts_, &ToastManager::Undo);
  // Once a leaving item is really gone the view may need to hide.
  connect(item, &QObject::destroyed, this, [this] {
    QMetaObject::invokeMethod(this, [this] { Place(); },
                              Qt::QueuedConnection);
  });
  layout_.addWidget(item);
  items_.push_back(item);
  item->show();
  Fade(item, 0.0, 1.0, false);
}

void ToastView::Place() {
  assert(anchor_ != nullptr);
  assert(host_ != nullptr);
  const bool is_showing =
      !findChildren<ToastItem*>(Qt::FindDirectChildrenOnly).isEmpty();
  setVisible(is_showing);
  if (!is_showing) {
    return;
  }
  const int width = std::min(kToastWidth, anchor_->width() - 2 * kSideGutter);
  layout_.activate();
  const int height = layout_.totalHeightForWidth(width);
  const QPoint bottom_middle =
      anchor_->mapTo(host_, QPoint(anchor_->width() / 2, anchor_->height()));
  setGeometry(bottom_middle.x() - width / 2,
              bottom_middle.y() - kBottomGap - height, width, height);
  raise();
}

}  // namespace snapper
