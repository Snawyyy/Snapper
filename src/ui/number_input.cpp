#include "ui/number_input.h"

#include <QEvent>
#include <QKeyEvent>
#include <QPalette>
#include <QTimer>
#include <QToolTip>

#include <cassert>
#include <cmath>
#include <memory>

#include "base/number_text.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kFlashMs = 450;

}  // namespace

void NumberInput::Install(QAbstractSpinBox* box) {
  assert(box != nullptr);
  assert(box->findChild<QLineEdit*>() != nullptr);
  const bool is_done = box->findChild<NumberInput*>() != nullptr;
  if (!is_done) {
    // The box owns it as its Qt child.
    std::make_unique<NumberInput>(box).release();
  }
}

NumberInput::NumberInput(QAbstractSpinBox* box)
    : QObject(box),
      box_(box),
      edit_(box->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly)) {
  assert(box_ != nullptr);
  assert(edit_ != nullptr);
  // The box's own check would refuse "*" before we ever saw it.
  edit_->setValidator(nullptr);
  // Math is applied whole, on Enter; per-key updates would rewrite the
  // text under the cursor ("+1" would turn into "1.0 deg").
  box_->setKeyboardTracking(false);
  // Keys and focus land on the box when it holds the keyboard and on
  // its line edit when that does; listen to both.
  edit_->installEventFilter(this);
  box_->installEventFilter(this);
}

QString NumberInput::Body() const {
  assert(box_ != nullptr);
  assert(edit_ != nullptr);
  QString text = edit_->text();
  const QString prefix = box_->property("prefix").toString();
  const QString suffix = box_->property("suffix").toString().trimmed();
  const bool has_prefix = !prefix.isEmpty() && text.startsWith(prefix);
  if (has_prefix) {
    text.remove(0, prefix.size());
  }
  const bool has_suffix = !suffix.isEmpty();
  if (has_suffix) {
    text.remove(suffix);
  }
  return text;
}

void NumberInput::Commit() {
  assert(box_ != nullptr);
  assert(edit_ != nullptr);
  const bool is_typed = edit_->isModified();
  const QString special = box_->property("specialValueText").toString();
  const bool is_special = !special.isEmpty() && edit_->text() == special;
  const bool is_untouched = !is_typed || is_special;
  if (is_untouched) {
    return;
  }
  const double current = box_->property("value").toDouble();
  const auto result = EvalNumber(Body(), current);
  const bool is_read = result.has_value();
  if (!is_read) {
    Refuse(edit_->text());
    return;
  }
  // Only a double box has decimals; a whole-number box rounds.
  const bool is_whole = !box_->property("decimals").isValid();
  box_->setProperty("value", is_whole ? std::round(*result) : *result);
  edit_->setModified(false);
}

void NumberInput::Refuse(const QString& typed) {
  assert(box_ != nullptr);
  assert(edit_ != nullptr);
  // Same value again: no change signal, but the old text comes back.
  box_->setProperty("value", box_->property("value"));
  edit_->setModified(false);
  QPalette bad = edit_->palette();
  bad.setColor(QPalette::Base, theme::kFieldBad);
  edit_->setPalette(bad);
  QLineEdit* edit = edit_;
  QTimer::singleShot(kFlashMs, edit,
                     [edit] { edit->setPalette(QPalette()); });
  const bool is_empty = typed.trimmed().isEmpty();
  const QString why =
      is_empty ? tr("Type a number. The field kept its value.")
               : tr("\"%1\" is not a number or a sum. The field kept its "
                    "value.")
                     .arg(typed.trimmed());
  QToolTip::showText(box_->mapToGlobal(QPoint(0, box_->height())), why,
                     box_);
}

bool NumberInput::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const QEvent::Type type = event->type();
  const bool is_ours = watched == edit_ || watched == box_;
  const bool is_key = is_ours && type == QEvent::KeyPress;
  const int key = is_key ? static_cast<QKeyEvent*>(event)->key() : 0;
  const bool is_enter = key == Qt::Key_Return || key == Qt::Key_Enter;
  const bool is_left = is_ours && type == QEvent::FocusOut;
  const bool is_commit = is_enter || is_left;
  if (is_commit) {
    Commit();
  }
  return QObject::eventFilter(watched, event);
}

}  // namespace snapper
