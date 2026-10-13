#ifndef SNAPPER_UI_SCROLL_FIT_H_
#define SNAPPER_UI_SCROLL_FIT_H_

class QScrollArea;

namespace snapper {

// The most of the screen's width a side panel may claim on its own.
inline constexpr double kWidestShare = 0.30;

// Keeps area at least as wide as the widget it scrolls needs, plus its
// scroll bar, whenever that widget's layout changes, so a column of
// settings that only scrolls up and down never clips on the right. It
// only grows, so the column keeps its width from pick to pick. It
// never asks for more than kWidestShare of the screen's width: past
// that the column scrolls sideways instead of crowding the stage.
void KeepWideEnough(QScrollArea* area);

// The least width to ask for: needed, capped at kWidestShare of
// screen_width (no cap when screen_width is 0, before a screen is
// known).
int CappedWidth(int needed, int screen_width);

}  // namespace snapper

#endif  // SNAPPER_UI_SCROLL_FIT_H_
