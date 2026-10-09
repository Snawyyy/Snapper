#ifndef SNAPPER_UI_TOP_BAR_H_
#define SNAPPER_UI_TOP_BAR_H_

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include "ui/bar_text.h"
#include "ui/managers.h"
#include "ui/mode_tabs.h"

namespace snapper {

// The bar across the top: the Pose, Rig and Video tabs, the transport
// (first frame, back, play or pause, forward, last frame, the frame
// counter and the time) and the Undo and Redo buttons. It shows what
// the playback and history managers say and forwards clicks to them;
// the words and states come from bar_text.h. The keys stay on the
// menu entries, so no button takes focus and Space still plays.
class TopBar final : public QWidget {
  Q_OBJECT

 public:
  explicit TopBar(const Managers& managers);

  ModeTabs* tabs() { return &tabs_; }
  int current_tab() const { return tabs_.currentIndex(); }
  // Puts a step's words, tooltip and enabled state on its button.
  void ShowUndo(const StepFace& face);
  void ShowRedo(const StepFace& face);

 private:
  // Puts the transport and the counters after the tabs.
  void BuildTransport();
  // Puts the Undo and Redo buttons after the transport.
  void BuildSteps();
  // Shows the playhead and whether the song is playing.
  void ShowPlayhead();
  // Makes a button that never takes focus and fades its hover.
  void AddButton(QPushButton* button, const QString& tip);

  Managers managers_;
  // The layout comes before what it holds, so the held widgets leave
  // it before it goes.
  QHBoxLayout layout_;
  ModeTabs tabs_;
  QPushButton first_;
  QPushButton back_;
  QPushButton play_;
  QPushButton next_;
  QPushButton last_;
  QLabel counter_;
  QLabel time_;
  QPushButton undo_;
  QPushButton redo_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOP_BAR_H_
