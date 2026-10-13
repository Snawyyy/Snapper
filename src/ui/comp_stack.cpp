#include "ui/comp_stack.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "fsp/fsp_effects.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kRowHeight = 38.0;
constexpr double kGap = 6.0;
constexpr double kEye = 14.0;
constexpr double kDragStart = 4.0;
// F's Plugins carries about 50; more is a broken list.
constexpr size_t kMaxPlugins = 512;

// A color per kind, for the kind mark.
QColor MarkColor(CompKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kCompKindCount);
  switch (kind) {
    case CompKind::kLineSmoother:
      return QColor(0x8a, 0x8a, 0x90);
    case CompKind::kColorSwap:
      return QColor(0xc0, 0x5a, 0xd0);
    case CompKind::kTransmittedLight:
      return QColor(0xf0, 0xb0, 0x50);
    case CompKind::kRimLight:
      return QColor(0xff, 0xe8, 0xb0);
    case CompKind::kGradient:
      return QColor(0x50, 0x90, 0xe0);
    case CompKind::kMotionBlur:
      return QColor(0x60, 0xc0, 0xa0);
    case CompKind::kSparkle:
      return QColor(0xff, 0xff, 0xff);
    case CompKind::kSpeedLines:
      return QColor(0x40, 0x40, 0x40);
    case CompKind::kFinish:
      return QColor(0xa0, 0x80, 0x60);
    case CompKind::kFsp:
      return QColor(0xe0, 0x70, 0x40);
  }
  return Qt::gray;
}

}  // namespace

CompStack::CompStack(const Managers& managers)
    : managers_(managers), add_(tr("+ Add effect")) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  add_.setParent(this);
  add_.setFocusPolicy(Qt::NoFocus);
  add_.move(static_cast<int>(kGap), static_cast<int>(kGap));
  BuildAddMenu();
  connect(&add_, &QPushButton::clicked, this, &CompStack::OpenAdd);
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.comp, &CompManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  Refresh();
}

void CompStack::BuildAddMenu() {
  assert(add_menu_.isEmpty());
  assert(managers_.comp != nullptr);
  add_menu_.setToolTipsVisible(true);
  for (int i = 0; i < kCompKindCount; ++i) {
    const auto kind = static_cast<CompKind>(i);
    // Plugins are listed by their own names below.
    const bool is_plugin = kind == CompKind::kFsp;
    if (is_plugin) {
      continue;
    }
    QAction* action = add_menu_.addAction(CompManager::KindName(kind));
    action->setToolTip(CompManager::KindHelp(kind));
    connect(action, &QAction::triggered, this, [this, kind] {
      Report(ProblemOf(
          managers_.comp->Add(managers_.selection->shot(), kind)));
    });
  }
  add_menu_.addSeparator();
  QAction* scene = add_menu_.addAction(tr("Scene gradient"));
  scene->setToolTip(tr("A gradient over the whole shot in one click: a "
                       "sunset tint, a dark vignette, a top-down light."));
  connect(scene, &QAction::triggered, this, [this] {
    Report(ProblemOf(
        managers_.comp->AddSceneGradient(managers_.selection->shot())));
  });
  BuildPluginMenu();
}

void CompStack::BuildPluginMenu() {
  assert(!add_menu_.isEmpty());
  const std::vector<FspEffectInfo>& plugins = AllFspEffects();
  assert(plugins.size() <= kMaxPlugins);
  add_menu_.addSeparator();
  QMenu* menu = add_menu_.addMenu(tr("F's Plugins"));
  menu->setToolTipsVisible(true);
  const bool is_empty = plugins.empty();
  menu->setEnabled(!is_empty);
  menu->menuAction()->setToolTip(
      is_empty ? tr("This Snapper was built without F's Plugins.")
               : CompManager::KindHelp(CompKind::kFsp));
  for (const FspEffectInfo& info : plugins) {
    const QString id = info.id;
    QAction* action =
        menu->addAction(info.name.isEmpty() ? info.id : info.name);
    Explain(action, CompManager::WhyNoPlugin(id));
    connect(action, &QAction::triggered, this, [this, id] {
      Report(ProblemOf(
          managers_.comp->AddPlugin(managers_.selection->shot(), id)));
    });
  }
}

void CompStack::OpenAdd() {
  const QString why = managers_.comp->WhyNoAdd(managers_.selection->shot());
  if (!why.isEmpty()) {
    Report(why);
    return;
  }
  add_menu_.popup(add_.mapToGlobal(QPoint(0, add_.height())));
}

void CompStack::Refresh() {
  Explain(&add_, managers_.comp->WhyNoAdd(managers_.selection->shot()),
          tr("Add a finishing effect on top of the stack (A)."));
  add_.resize(std::max(add_.sizeHint().width(), width() - 2 * static_cast<int>(kGap)),
              add_.sizeHint().height());
  const int rows = static_cast<int>(Rows().size());
  setMinimumHeight(static_cast<int>(Top() + rows * kRowHeight + kGap));
  update();
}

std::vector<CompEffect> CompStack::Rows() const {
  std::vector<CompEffect> rows =
      managers_.comp->Of(managers_.selection->shot());
  std::ranges::reverse(rows);
  return rows;
}

double CompStack::Top() const {
  return kGap * 2.0 + add_.sizeHint().height();
}

QRectF CompStack::RowRect(int row) const {
  assert(row >= 0);
  return QRectF(kGap, Top() + row * kRowHeight, width() - 2.0 * kGap,
                kRowHeight - 2.0);
}

QRectF CompStack::EyeRect(int row) const {
  const QRectF box = RowRect(row);
  return QRectF(box.left() + 6.0, box.center().y() - kEye / 2.0, kEye, kEye);
}

int CompStack::RowAt(double y) const {
  const int count = static_cast<int>(Rows().size());
  const int row = static_cast<int>(std::floor((y - Top()) / kRowHeight));
  const bool is_row = y >= Top() && row >= 0 && row < count;
  return is_row ? row : -1;
}

int CompStack::GapAt(double y) const {
  const int count = static_cast<int>(Rows().size());
  const int gap =
      static_cast<int>(std::lround((y - Top()) / kRowHeight));
  return std::clamp(gap, 0, count);
}

void CompStack::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), QColor(0x2a, 0x2a, 0x2d));
  const std::vector<CompEffect> rows = Rows();
  const ShotId shot = managers_.selection->shot();
  const CompEffectId picked = managers_.comp->PickedIn(shot);
  const bool is_empty = rows.empty();
  if (is_empty) {
    painter.setPen(QColor(0x80, 0x80, 0x86));
    painter.drawText(QRectF(kGap, Top(), width() - 2 * kGap, 60),
                     Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     tr("No effects yet. Press A or + to add one."));
  }
  for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
    const CompEffect& effect = rows[static_cast<size_t>(row)];
    const QRectF box = RowRect(row);
    const bool is_picked = effect.id == picked;
    painter.fillRect(box, QColor(0x3a, 0x3a, 0x3e));
    painter.setPen(is_picked ? QPen(theme::kPick, 2.0)
                             : QPen(QColor(0x10, 0x10, 0x12), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box.adjusted(1, 1, -1, -1));
    // The eye: filled when on.
    const QRectF eye = EyeRect(row);
    painter.setPen(QColor(0xe6, 0xe6, 0xe6));
    painter.setBrush(effect.is_on ? QColor(0xe6, 0xe6, 0xe6)
                                  : QColor(Qt::transparent));
    painter.drawRect(eye);
    const QRectF mark(eye.right() + 6.0, eye.top(), kEye, kEye);
    painter.setPen(Qt::black);
    painter.setBrush(MarkColor(effect.kind));
    painter.drawRect(mark);
    const double text_left = mark.right() + 6.0;
    const QRectF name(text_left, box.top() + 2, box.right() - text_left - 22,
                      box.height() / 2);
    painter.setPen(effect.is_on ? QColor(0xe6, 0xe6, 0xe6)
                                : QColor(0x80, 0x80, 0x86));
    painter.drawText(name, Qt::AlignLeft | Qt::AlignVCenter,
                     CompManager::EffectName(effect));
    const QRectF target(text_left, box.center().y(),
                        box.right() - text_left - 22, box.height() / 2 - 2);
    painter.setPen(QColor(0x9a, 0x9a, 0xa0));
    painter.drawText(target, Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(
                         managers_.comp->TargetText(shot, effect),
                         Qt::ElideRight, static_cast<int>(target.width())));
    const bool is_slow = managers_.comp->IsSlow(effect.id);
    if (is_slow) {
      // A small clock: it costs more than a frame at 24 fps.
      const QPointF middle(box.right() - 12, box.center().y());
      painter.setPen(QPen(QColor(0xf2, 0xc2, 0x30), 1.5));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(middle, 6, 6);
      painter.drawLine(middle, middle + QPointF(0, -4));
      painter.drawLine(middle, middle + QPointF(3, 0));
    }
  }
  if (is_dragging_ && drop_gap_ >= 0) {
    const double y = Top() + drop_gap_ * kRowHeight - 1.0;
    painter.fillRect(QRectF(kGap, y - 1, width() - 2 * kGap, 3), theme::kPick);
  }
}

void CompStack::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const int row = RowAt(event->position().y());
  const bool is_row = row >= 0;
  if (!is_row) {
    return;
  }
  const CompEffect effect = Rows()[static_cast<size_t>(row)];
  const ShotId shot = managers_.selection->shot();
  const bool is_eye = EyeRect(row).adjusted(-4, -4, 4, 4).contains(
      event->position());
  if (is_eye) {
    Report(ProblemOf(managers_.comp->SetOn(shot, effect.id, !effect.is_on)));
    return;
  }
  managers_.comp->Pick(shot, effect.id);
  const bool is_left = event->button() == Qt::LeftButton;
  pressed_row_ = is_left ? row : -1;
  press_at_ = event->position();
}

void CompStack::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_pressed = pressed_row_ >= 0;
  if (!is_pressed) {
    return;
  }
  const bool is_far =
      QLineF(press_at_, event->position()).length() > kDragStart;
  is_dragging_ = is_dragging_ || is_far;
  if (is_dragging_) {
    drop_gap_ = GapAt(event->position().y());
    update();
  }
}

void CompStack::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_drop = is_dragging_ && pressed_row_ >= 0 && drop_gap_ >= 0;
  if (is_drop) {
    const std::vector<CompEffect> rows = Rows();
    const int count = static_cast<int>(rows.size());
    const int shown_at =
        drop_gap_ > pressed_row_ ? drop_gap_ - 1 : drop_gap_;
    const bool is_moved = shown_at != pressed_row_;
    if (is_moved) {
      Report(ProblemOf(managers_.comp->Move(
          managers_.selection->shot(),
          rows[static_cast<size_t>(pressed_row_)].id, count - 1 - shown_at)));
    }
  }
  pressed_row_ = -1;
  is_dragging_ = false;
  drop_gap_ = -1;
  update();
}

void CompStack::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_cancel = event->key() == Qt::Key_Escape && is_dragging_;
  if (is_cancel) {
    pressed_row_ = -1;
    is_dragging_ = false;
    drop_gap_ = -1;
    update();
    return;
  }
  QWidget::keyPressEvent(event);
}

void CompStack::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  const int row = RowAt(event->pos().y());
  const bool is_row = row >= 0;
  if (!is_row) {
    return;
  }
  const CompEffectId id = Rows()[static_cast<size_t>(row)].id;
  const ShotId shot = managers_.selection->shot();
  managers_.comp->Pick(shot, id);
  QMenu menu;
  QAction* duplicate = menu.addAction(tr("Duplicate\tCtrl+D"));
  QAction* remove = menu.addAction(tr("Remove\tDelete"));
  Explain(duplicate, managers_.comp->WhyNoAdd(shot));
  const QAction* chosen = menu.exec(event->globalPos());
  if (chosen == duplicate) {
    Report(ProblemOf(managers_.comp->Duplicate(shot, id)));
  } else if (chosen == remove) {
    Report(ProblemOf(managers_.comp->Remove(shot, id)));
  }
}

}  // namespace snapper
