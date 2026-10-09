#include "ui/scrub_label.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>

#include "ui/live_edit.h"

namespace snapper {
namespace {

// What one pixel of dragging adds when a field does not say.
constexpr double kDefaultUnit = 1.0;

}  // namespace

ScrubLabel::ScrubLabel(const QString& text, QAbstractSpinBox* box,
                       LiveEdit* live)
    : QLabel(text), box_(box), live_(live) {
  assert(box_ != nullptr);
  assert(!text.isEmpty());
  setCursor(Qt::SizeHorCursor);
  box_->installEventFilter(this);
  SyncWithBox();
}

double ScrubLabel::ValueOfBox() const {
  assert(box_ != nullptr);
  assert(box_->property("value").isValid());
  return box_->property("value").toDouble();
}

void ScrubLabel::PutInBox(double value) {
  assert(box_ != nullptr);
  assert(std::isfinite(value));
  box_->setProperty("value", value);
}

ScrubRange ScrubLabel::RangeOfBox(double unit) const {
  assert(box_ != nullptr);
  assert(unit > 0.0);
  // Only a double box has decimals; a whole-number box has none.
  return ScrubRange{unit, box_->property("decimals").toInt(),
                    box_->property("minimum").toDouble(),
                    box_->property("maximum").toDouble()};
}

void ScrubLabel::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!drag_.has_value());
  const bool is_other_button = event->button() != Qt::LeftButton;
  if (is_other_button) {
    QLabel::mousePressEvent(event);
    return;
  }
  const QVariant unit = box_->property("scrub_unit");
  const double per_pixel = unit.isValid() ? unit.toDouble() : kDefaultUnit;
  drag_.emplace(ValueOfBox(), RangeOfBox(per_pixel));
  press_x_ = static_cast<int>(event->globalPosition().x());
}

void ScrubLabel::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  const bool is_idle = !drag_.has_value();
  if (is_idle) {
    return;
  }
  const bool was_active = drag_->is_active();
  const int dx = static_cast<int>(event->globalPosition().x()) - press_x_;
  const Qt::KeyboardModifiers held = event->modifiers();
  const ScrubKeys keys{held.testFlag(Qt::ShiftModifier),
                       held.testFlag(Qt::AltModifier)};
  const double value = drag_->Move(dx, keys);
  const bool is_started = drag_->is_active() && !was_active;
  if (is_started) {
    // Escape has to reach the drag, not the dialog around it.
    grabKeyboard();
  }
  const bool is_active = drag_->is_active();
  if (is_active) {
    PutInBox(value);
  }
}

void ScrubLabel::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  const bool is_idle = !drag_.has_value();
  if (is_idle) {
    return;
  }
  const bool is_click = !drag_->is_active();
  End(true);
  if (is_click) {
    box_->setFocus();
    box_->selectAll();
  }
}

void ScrubLabel::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool is_cancel = is_escape && drag_.has_value();
  if (is_cancel) {
    End(false);
    return;
  }
  QLabel::keyPressEvent(event);
}

void ScrubLabel::End(bool is_kept) {
  assert(drag_.has_value());
  assert(box_ != nullptr);
  const double start = drag_->start();
  const bool was_active = drag_->is_active();
  drag_.reset();
  releaseKeyboard();
  const bool can_finish = is_kept && live_ != nullptr;
  const bool can_cancel = !is_kept && live_ != nullptr;
  const bool must_restore = !is_kept && was_active;
  if (can_finish) {
    live_->Finish();
  }
  if (can_cancel) {
    live_->Cancel();
  }
  if (must_restore) {
    // The project is back; put the number back too, quietly.
    const QSignalBlocker quiet(box_);
    PutInBox(start);
  }
}

void ScrubLabel::SyncWithBox() {
  assert(box_ != nullptr);
  assert(!text().isEmpty());
  const bool is_usable = box_->isEnabled();
  const QString reason = box_->toolTip();
  const QString how = tr("Drag sideways to change it. Shift is 10 times "
                         "faster, Alt is 10 times finer. Escape cancels.");
  const bool is_plain = is_usable && reason.isEmpty();
  setEnabled(is_usable);
  if (is_plain) {
    setToolTip(how);
  } else if (is_usable) {
    setToolTip(reason + "\n\n" + how);
  } else {
    setToolTip(reason);
  }
}

bool ScrubLabel::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const QEvent::Type type = event->type();
  const bool is_change =
      watched == box_ && (type == QEvent::EnabledChange ||
                          type == QEvent::ToolTipChange);
  if (is_change) {
    SyncWithBox();
  }
  return QLabel::eventFilter(watched, event);
}

}  // namespace snapper
