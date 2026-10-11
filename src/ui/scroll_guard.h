#ifndef SNAPPER_UI_SCROLL_GUARD_H_
#define SNAPPER_UI_SCROLL_GUARD_H_

class QAbstractScrollArea;
class QWidget;

namespace snapper {

// A panel that scrolls must not let a dial or slider under the cursor
// grab the wheel half way through a scroll. Watch the scroll area, and
// the controls ask IsMidScroll before they take a wheel event.

// Notes the time whenever area scrolls.
void WatchScrolling(QAbstractScrollArea* area);

// True while a watched scroll area above widget scrolled a moment ago.
bool IsMidScroll(const QWidget* widget);

}  // namespace snapper

#endif  // SNAPPER_UI_SCROLL_GUARD_H_
