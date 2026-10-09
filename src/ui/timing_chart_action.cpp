#include "ui/timing_chart_action.h"

#include <QAction>

#include <cassert>

#include "edit/timing_chart_manager.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {

QKeySequence TimingChartShortcut() {
  assert(Qt::Key_T > 0);
  assert(Qt::SHIFT != 0);
  return QKeySequence(Qt::SHIFT | Qt::Key_T);
}

void RequestTimingChart(const Managers& managers) {
  assert(managers.timing != nullptr);
  assert(managers.toasts != nullptr);
  managers.toasts->Problem(ProblemOf(managers.timing->Open()));
}

void AddTimingChartAction(QMenu* menu, const Managers& managers) {
  assert(menu != nullptr);
  assert(managers.timing != nullptr);
  const QString hotkey =
      TimingChartShortcut().toString(QKeySequence::NativeText);
  QAction* action = menu->addAction(
      QObject::tr("Define timing chart...\t%1").arg(hotkey));
  const QString why_not = managers.timing->WhyNoChart();
  action->setEnabled(why_not.isEmpty());
  action->setToolTip(
      why_not.isEmpty()
          ? QObject::tr("Space the in-between frames from the first picked "
                        "frame to the last.")
          : why_not);
  QObject::connect(action, &QAction::triggered, menu,
                   [managers] { RequestTimingChart(managers); });
}

}  // namespace snapper
