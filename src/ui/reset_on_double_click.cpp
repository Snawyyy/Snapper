#include "ui/reset_on_double_click.h"

#include <QEvent>
#include <QLineEdit>

#include <cassert>
#include <cmath>
#include <memory>

namespace snapper {

void ResetOnDoubleClick::Install(QDoubleSpinBox* box, double neutral,
                                 LiveEdit* live) {
  assert(box != nullptr);
  assert(std::isfinite(neutral));
  // The box owns it as its Qt child.
  std::make_unique<ResetOnDoubleClick>(box, neutral, live).release();
}

ResetOnDoubleClick::ResetOnDoubleClick(QDoubleSpinBox* box, double neutral,
                                       LiveEdit* live)
    : QObject(box), drag_({box}, live), neutral_(neutral) {
  assert(box != nullptr);
  assert(std::isfinite(neutral_));
  // The click lands on the box or on its line edit; listen to both.
  box->installEventFilter(this);
  QLineEdit* edit =
      box->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
  const bool has_edit = edit != nullptr;
  if (has_edit) {
    edit->installEventFilter(this);
  }
}

bool ResetOnDoubleClick::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const bool is_double = event->type() == QEvent::MouseButtonDblClick;
  if (!is_double) {
    return QObject::eventFilter(watched, event);
  }
  const bool can_reset = drag_.is_enabled() && !drag_.is_active();
  if (can_reset) {
    drag_.SetAll({neutral_});
  }
  return true;
}

}  // namespace snapper
