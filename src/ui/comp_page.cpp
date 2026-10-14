#include "ui/comp_page.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <cassert>

#include "anim/reel_timeline.h"
#include "edit/document_manager.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/problem.h"

namespace snapper {
namespace {

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
      library_(managers),
      stage_(managers),
      transport_(managers),
      timeline_(managers),
      effects_(managers) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  library_.setMinimumWidth(200);
  split_.addWidget(&library_);
  center_.addWidget(&stage_);
  bottom_layout_.setContentsMargins(0, 0, 0, 0);
  bottom_layout_.setSpacing(0);
  bottom_layout_.addWidget(&transport_, 0, Qt::AlignLeft);
  bottom_layout_.addWidget(&timeline_, 1);
  center_.addWidget(&bottom_);
  center_.SetDefaultShares({3, 1});
  center_.Remember(QStringLiteral("comp_center"),
                   managers_.document->SettingsPath());
  split_.addWidget(&center_);
  effects_scroll_.setWidgetResizable(true);
  effects_scroll_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  effects_scroll_.setFrameShape(QFrame::NoFrame);
  effects_scroll_.setWidget(&effects_);
  effects_scroll_.setMinimumWidth(280);
  split_.addWidget(&effects_scroll_);
  split_.SetDefaultShares({0, 3, 1});
  split_.Remember(QStringLiteral("comp_main"),
                  managers_.document->SettingsPath());
  layout_.addWidget(&split_);
  const auto report = [this](const QString& why) { Report(why); };
  connect(&library_, &CompLibrary::Problem, this, report);
  connect(&stage_, &CompStage::Problem, this, report);
  connect(&effects_, &CompEffects::Problem, this, report);
  connect(&effects_, &CompEffects::Reveal, this, [this](QWidget* fold) {
    effects_scroll_.ensureWidgetVisible(fold, 0, 40);
  });
  const auto follow = [this] { FollowPlayhead(); };
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, follow);
  connect(managers_.history, &HistoryManager::Changed, this, follow);
  qApp->installEventFilter(this);
}

void CompPage::FollowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(managers_.selection != nullptr);
  if (!isVisible()) {
    return;
  }
  const ReelShot at =
      ShotAtReel(managers_.history->current(),
                 managers_.playback->FrameOn(Timeline::kReel));
  if (at.IsValid()) {
    managers_.selection->SelectShot(at.shot);
  }
}

void CompPage::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  FollowPlayhead();
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
  const bool is_control = key.modifiers().testFlag(Qt::ControlModifier);
  const bool is_clip = is_control && (key.key() == Qt::Key_D ||
                                      key.key() == Qt::Key_C ||
                                      key.key() == Qt::Key_V);
  return is_letter || is_delete || is_clip || IsStepUp(key) ||
         IsStepDown(key);
}

void CompPage::PressKey(const QKeyEvent& key) {
  assert(managers_.comp != nullptr);
  CompManager* comp = managers_.comp;
  const ShotId shot = managers_.selection->shot();
  const CompEffectId picked = comp->PickedIn(shot);
  const bool has_pick = picked.IsValid();
  const bool is_control = key.modifiers().testFlag(Qt::ControlModifier);
  const bool is_paste = is_control && key.key() == Qt::Key_V;
  if (is_paste) {
    effects_.Paste();
    return;
  }
  switch (key.key()) {
    case Qt::Key_A:
      library_.FocusSearch();
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
  const bool is_copy = key.key() == Qt::Key_C;
  if (is_copy) {
    const auto copied = comp->Copy(shot, picked);
    Report(ProblemOf(copied));
    const bool is_copied = copied.has_value();
    if (is_copied) {
      stage_.ShowChip(tr("Copied %1").arg(comp->CopiedName()));
    }
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
