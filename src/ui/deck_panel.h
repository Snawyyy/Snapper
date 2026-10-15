#ifndef SNAPPER_UI_DECK_PANEL_H_
#define SNAPPER_UI_DECK_PANEL_H_

#include <QWidget>

class QPaintEvent;

namespace snapper {

// A strip of a deck's front panel (deck::PaintPanel) for the bars over
// the timelines: whatever is laid out on it sits on brushed plastic.
// Room kept clear at a panel's ends for its screws.
inline constexpr int kDeckEdge = 14;

class DeckPanel final : public QWidget {
  Q_OBJECT

 public:
  DeckPanel() = default;

 protected:
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DECK_PANEL_H_
