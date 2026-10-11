#ifndef SNAPPER_UI_TRANSPORT_BAR_H_
#define SNAPPER_UI_TRANSPORT_BAR_H_

#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "ui/bump_label.h"
#include "ui/managers.h"

namespace snapper {

// The play controls: first frame, back, play or pause, forward, last
// frame, the frame counter and the time. One sits above each timeline
// (pose and video), so every copy shows the same playhead. It shows
// what the playback manager says and forwards clicks to it; the words
// come from bar_text.h. No button takes focus, so Space and the arrow
// keys stay on the menu entries.
class TransportBar final : public QWidget {
  Q_OBJECT

 public:
  explicit TransportBar(const Managers& managers);

 private:
  // Shows the playhead and whether the song is playing.
  void ShowPlayhead();
  // Makes a button that never takes focus and fades its hover.
  void AddButton(QPushButton* button, const QString& tip);

  Managers managers_;
  // The layout comes before what it holds, so the held widgets leave
  // it before it goes.
  QHBoxLayout layout_;
  QPushButton first_;
  QPushButton back_;
  QPushButton play_;
  QPushButton next_;
  QPushButton last_;
  BumpLabel counter_;
  BumpLabel time_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TRANSPORT_BAR_H_
