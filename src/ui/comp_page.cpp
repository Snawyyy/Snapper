#include "ui/comp_page.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <cassert>

#include "edit/comp_manager.h"
#include "edit/selection_manager.h"
#include "ui/golden_splitter.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The least widths of the effect stack and the settings panel, in ems.
constexpr int kStackEms = 12;
constexpr int kPanelEms = 17;

// True while a text or number field has the keys.
bool IsTyping() {
  const QWidget* focus = QApplication::focusWidget();
  return focus != nullptr && (qobject_cast<const QLineEdit*>(focus) ||
                              qobject_cast<const QAbstractSpinBox*>(focus) ||
                              qobject_cast<const QTextEdit*>(focus) ||
                              qobject_cast<const QPlainTextEdit*>(focus));
}

bool IsStepUp(const QKeyEvent& key) {
  return key.key() == Qt::Key_Plus ||
         (key.key() == Qt::Key_Equal &&
          key.modifiers().testFlag(Qt::ShiftModifier));
}

bool IsStepDown(const QKeyEvent& key) {
  return key.key() == Qt::Key_Underscore ||
         (key.key() == Qt::Key_Minus &&
          key.modifiers().testFlag(Qt::ShiftModifier));
}

}  // namespace

CompPage::CompPage(const Managers& managers)
    : managers_(managers),
      layout_(this),
      split_(Qt::Horizontal),
      center_(Qt::Vertical),
      bottom_layout_(&bottom_),
      stack_(managers),
      stage_(managers),
      transport_(managers),
      timeline_(managers),
      panel_(managers) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  stack_scroll_.setWidgetResizable(true);
  stack_scroll_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  stack_scroll_.setFrameShape(QFrame::NoFrame);
  stack_scroll_.setWidget(&stack_);
  // Narrow sides in ems, so they follow the font and the screen's scale.
  const int em = theme::Em(this);
  stack_scroll_.setMinimumWidth(kStackEms * em);
  split_.addWidget(&stack_scroll_);
  center_.addWidget(&stage_);
  bottom_layout_.setContentsMargins(0, 0, 0, 0);
  bottom_layout_.setSpacing(0);
  bottom_layout_.addWidget(&transport_, 0, Qt::AlignLeft);
  bottom_layout_.addWidget(&timeline_, 1);
  center_.addWidget(&bottom_);
  // The stage over the timeline by the golden ratio; the stage column
  // takes the long part of the width, the stack and the panel share the
  // rest. Named, so the window keeps them as they were left.
  center_.setObjectName(QStringLiteral("comp_center"));
  SplitGolden(&center_, 0);
  split_.addWidget(&center_);
  panel_scroll_.setWidgetResizable(true);
  panel_scroll_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  panel_scroll_.setFrameShape(QFrame::NoFrame);
  panel_scroll_.setWidget(&panel_);
  panel_scroll_.setMinimumWidth(kPanelEms * em);
  split_.addWidget(&panel_scroll_);
  split_.setObjectName(QStringLiteral("comp"));
  SplitGoldenAround(&split_);
  layout_.addWidget(&split_);
  const auto report = [this](const QString& why) { Report(why); };
  connect(&stack_, &CompStack::Problem, this, report);
  connect(&stage_, &CompStage::Problem, this, report);
  connect(&panel_, &CompPanel::Problem, this, report);
  qApp->installEventFilter(this);
}

CompPage::~CompPage() { qApp->removeEventFilter(this); }

bool CompPage::IsPageKey(const QKeyEvent& key) const {
  const bool is_plain = !key.modifiers().testFlag(Qt::ControlModifier) &&
                        !key.modifiers().testFlag(Qt::AltModifier);
  const bool is_letter =
      is_plain && (key.key() == Qt::Key_A || key.key() == Qt::Key_V ||
                   key.key() == Qt::Key_K || key.key() == Qt::Key_I);
  const bool is_delete =
      key.key() == Qt::Key_Delete || key.key() == Qt::Key_Backspace;
  const bool is_duplicate = key.key() == Qt::Key_D &&
                            key.modifiers().testFlag(Qt::ControlModifier);
  return is_letter || is_delete || is_duplicate || IsStepUp(key) ||
         IsStepDown(key);
}

void CompPage::PressKey(const QKeyEvent& key) {
  assert(managers_.comp != nullptr);
  CompManager* comp = managers_.comp;
  const ShotId shot = managers_.selection->shot();
  const CompEffectId picked = comp->PickedIn(shot);
  const bool has_pick = picked.IsValid();
  switch (key.key()) {
    case Qt::Key_A:
      stack_.OpenAdd();
      return;
    case Qt::Key_V:
      comp->SetEffectsOff(true);
      return;
    case Qt::Key_K:
      if (!has_pick) {
        Report(tr("Pick an effect to see what it touches."));
        return;
      }
      comp->SetMaskShown(true);
      return;
    case Qt::Key_I: {
      if (!has_pick) {
        Report(tr("Pick an effect to give it target colors."));
        return;
      }
      const QColor color = stage_.ColorUnderCursor();
      if (!color.isValid()) {
        Report(tr("Point at the frame to pick a color there."));
        return;
      }
      Report(ProblemOf(
          comp->PickTargetColor(shot, picked, color, key.modifiers())));
      return;
    }
    default:
      break;
  }
  if (!has_pick) {
    Report(tr("Pick an effect first."));
    return;
  }
  const bool is_duplicate = key.key() == Qt::Key_D;
  if (is_duplicate) {
    Report(ProblemOf(comp->Duplicate(shot, picked)));
    return;
  }
  const bool is_step = IsStepUp(key) || IsStepDown(key);
  if (is_step) {
    Report(ProblemOf(comp->StepBlend(shot, picked, IsStepUp(key) ? 1 : -1)));
    const CompEffect* effect = comp->Find(shot, picked);
    if (effect != nullptr) {
      stage_.ShowChip(CompManager::BlendName(effect->blend));
    }
    return;
  }
  Report(ProblemOf(comp->Remove(shot, picked)));
}

void CompPage::ReleaseKey(const QKeyEvent& key) {
  assert(managers_.comp != nullptr);
  if (key.key() == Qt::Key_V) {
    managers_.comp->SetEffectsOff(false);
  }
  if (key.key() == Qt::Key_K) {
    managers_.comp->SetMaskShown(false);
  }
}

bool CompPage::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  const QEvent::Type type = event->type();
  const bool is_key = type == QEvent::KeyPress ||
                      type == QEvent::KeyRelease ||
                      type == QEvent::ShortcutOverride;
  if (!is_key || !isVisible() || QApplication::activeWindow() != window()) {
    return QWidget::eventFilter(watched, event);
  }
  const auto* key = static_cast<const QKeyEvent*>(event);
  const bool is_ours = IsPageKey(*key) && !IsTyping();
  if (!is_ours) {
    return QWidget::eventFilter(watched, event);
  }
  if (type == QEvent::ShortcutOverride) {
    // The page takes the key before any menu shortcut does.
    event->accept();
    return true;
  }
  const bool is_hold = key->key() == Qt::Key_V || key->key() == Qt::Key_K;
  if (type == QEvent::KeyRelease) {
    if (is_hold && !key->isAutoRepeat()) {
      ReleaseKey(*key);
    }
    return true;
  }
  if (!key->isAutoRepeat()) {
    PressKey(*key);
  }
  return true;
}

}  // namespace snapper
