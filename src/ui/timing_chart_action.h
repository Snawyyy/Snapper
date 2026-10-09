#ifndef SNAPPER_UI_TIMING_CHART_ACTION_H_
#define SNAPPER_UI_TIMING_CHART_ACTION_H_

#include <QKeySequence>
#include <QMenu>

#include "ui/managers.h"

namespace snapper {

// The hotkey that opens the timing chart, everywhere.
QKeySequence TimingChartShortcut();

// Opens the timing chart, or toasts why it can't.
void RequestTimingChart(const Managers& managers);

// Adds "Define timing chart..." to menu, showing the hotkey. It is
// greyed with the reason as its tooltip when no chart can open.
void AddTimingChartAction(QMenu* menu, const Managers& managers);

}  // namespace snapper

#endif  // SNAPPER_UI_TIMING_CHART_ACTION_H_
