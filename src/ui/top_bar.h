#ifndef SNAPPER_UI_TOP_BAR_H_
#define SNAPPER_UI_TOP_BAR_H_

#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

#include "ui/bar_text.h"
#include "ui/managers.h"
#include "ui/mode_tabs.h"
#include "ui/step_button.h"

namespace snapper {

// The bar across the top: the Pose, Rig, Comp and Video tabs and the Undo
// and Redo buttons. It shows what the history manager says and forwards
// clicks to it; the words and states come from bar_text.h. The keys stay
// on the menu entries, so no button takes focus. The play controls live
// above the timelines (TransportBar).
class TopBar final : public QWidget {
  Q_OBJECT

 public:
  explicit TopBar(const Managers& managers);

  ModeTabs* tabs() { return &tabs_; }
  int current_tab() const { return tabs_.currentIndex(); }
  // Puts a step's words, tooltip and enabled state on its button.
  void ShowUndo(const StepFace& face);
  void ShowRedo(const StepFace& face);

 signals:
  // The Undo or Redo button was pressed; the window decides what it
  // undoes (the shot being worked in, or the newest change).
  void UndoRequested();
  void RedoRequested();

 private:
  // Puts the Undo and Redo buttons at the bar's right end.
  void BuildSteps();
  // Makes a button that never takes focus and fades its hover.
  void AddButton(QPushButton* button, const QString& tip);

  Managers managers_;
  // The layout comes before what it holds, so the held widgets leave
  // it before it goes.
  QHBoxLayout layout_;
  ModeTabs tabs_;
  StepButton undo_;
  StepButton redo_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOP_BAR_H_
