#ifndef SNAPPER_UI_COMP_EFFECTS_H_
#define SNAPPER_UI_COMP_EFFECTS_H_

#include <QAbstractButton>
#include <QFrame>
#include <QLabel>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <set>
#include <utility>
#include <vector>

#include "edit/comp_manager.h"
#include "model/comp.h"
#include "ui/comp_light.h"
#include "ui/managers.h"

class QContextMenuEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;

namespace snapper {

// The Comp tab's right side: the effects on what is picked on the
// stage (the screen's when nothing is), in the order they run, each a
// fold like in After Effects. Its plate shows the kind's mark, the on
// switch, the name and the fold arrow; a click on the name picks the
// effect (for K, I, Delete and the palette) and opens or closes it.
// Plates drag to reorder (Escape cancels); library entries dropped
// between them are added there. Right click copies, pastes, duplicates
// or removes. The shot's Light sits under them.
class CompEffects final : public QWidget {
  Q_OBJECT

 public:
  explicit CompEffects(const Managers& managers);

  // Ctrl+V: the copied effect on top of the pick's effects.
  void Paste();

 signals:
  void Problem(const QString& why);
  // A new effect's fold, for the scroll area to bring into view.
  void Reveal(QWidget* fold);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  // The plate widgets kept to show the pick, eye and cost in place.
  struct Plate final {
    CompEffectId id{};
    // A PlateFrame (ui/comp_effects.cpp): red edged while picked.
    QFrame* plate = nullptr;
    QFrame* body = nullptr;
    QToolButton* arrow = nullptr;
    QAbstractButton* eye = nullptr;
    QLabel* name = nullptr;
    QLabel* slow = nullptr;
  };

  void Report(const QString& problem) {
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }
  void Refresh();
  // Rebuilds the folds for the effects listed now.
  void Rebuild(const std::vector<CompEffect>& shown);
  void AddFold(const CompEffect& effect, QVBoxLayout* into);
  QFrame* NewPlate(const CompEffect& effect, Plate* plate);
  // The sunken body under a plate, holding the effect's controls.
  QFrame* NewBody(CompEffectId id);
  void Toggle(const Plate& plate);
  // Shows the pick, eyes and slow marks without rebuilding.
  void ShowPlates();
  void OpenMenu(CompEffectId id, const QPoint& at);
  // Drags (ui/comp_effects_drop.cpp).
  void StartDrag(const Plate& plate);
  // The gap between listed effects at y (0 to the count).
  int GapAt(double y) const;
  // The index in the whole stack that the gap stands for.
  int StackIndexOf(int gap) const;
  void ShowDropLine(int gap);
  void MoveToGap(CompEffectId id, int gap);
  void PasteAt(int index);
  const Plate* PlateOf(const QObject* widget) const;
  CompTarget PickedTarget() const;
  std::vector<CompEffect> Shown() const;
  // What the folds are built for; a change rebuilds them.
  QString Signature(const std::vector<CompEffect>& shown) const;

  Managers managers_;
  QVBoxLayout layout_;
  QLabel title_;
  QWidget* folds_ = nullptr;
  QWidget light_body_;
  QVBoxLayout light_layout_;
  CompLight light_;
  QFrame drop_line_;
  std::vector<Plate> plates_;
  // Folds the user closed, by shot and effect; new effects open.
  std::set<std::pair<int, int>> closed_;
  QString signature_;
  // The plate pressed, for a click or a drag.
  CompEffectId pressed_{};
  QPointF press_at_;
  // A plate dropped while its own drag runs, moved once the drag ends.
  CompEffectId moving_{};
  int moving_gap_ = -1;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_EFFECTS_H_
