#ifndef SNAPPER_UI_SCROLL_FIT_H_
#define SNAPPER_UI_SCROLL_FIT_H_

class QScrollArea;

namespace snapper {

// Keeps area at least as wide as the widget it scrolls needs, plus its
// scroll bar, whenever that widget's layout changes, so a column of
// settings that only scrolls up and down never clips on the right.
void KeepWideEnough(QScrollArea* area);

}  // namespace snapper

#endif  // SNAPPER_UI_SCROLL_FIT_H_
