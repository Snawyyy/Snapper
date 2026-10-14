#ifndef SNAPPER_UI_COMP_EFFECTS_H_
#define SNAPPER_UI_COMP_EFFECTS_H_

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QPoint>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <set>
#include <utility>
#include <vector>

#include "model/comp.h"
#include "ui/comp_light.h"
#include "ui/managers.h"

namespace snapper {

// The Comp tab's right side: the effects on what is picked on the
// stage (the screen's when nothing is), in the order they run, each a
// fold like in After Effects. Its plate shows an arrow that folds it,
// the eye, and the name, which picks the effect (for K, I, Delete and
// the palette); right click moves, duplicates or removes it. The open
// body holds the effect's controls. The shot's Light sits under them.
class CompEffects final : public QWidget {
  Q_OBJECT

 public:
  explicit CompEffects(const Managers& managers);

 signals:
  void Problem(const QString& why);

 private:
  // The plate widgets kept to show the pick, eye and cost in place.
  struct Plate final {
    CompEffectId id{};
    QCheckBox* eye = nullptr;
    QToolButton* name = nullptr;
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
  // The sunken body under a plate, holding the effect's controls.
  QFrame* NewBody(CompEffectId id);
  // Shows the pick, eyes and slow marks without rebuilding.
  void ShowPlates();
  void OpenMenu(CompEffectId id, const QPoint& at);
  // Moves id past its listed neighbour, earlier (-1) or later (1).
  void Step(CompEffectId id, int direction);
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
  std::vector<Plate> plates_;
  // Folds the user closed, by shot and effect; new effects open.
  std::set<std::pair<int, int>> closed_;
  QString signature_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_EFFECTS_H_
