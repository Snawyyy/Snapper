#ifndef SNAPPER_UI_COMP_LIBRARY_H_
#define SNAPPER_UI_COMP_LIBRARY_H_

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPoint>
#include <QSet>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <optional>
#include <utility>
#include <vector>

#include "edit/comp_manager.h"
#include "ui/effect_row_delegate.h"
#include "ui/managers.h"

namespace snapper {

// The Comp tab's left side: the recently used effects, then every
// effect Snapper has, its own kinds with the Scene gradient, then F's
// Plugins, each with its color mark,
// under a search field. Hovering one shows what it does; a double click
// or Enter adds it on what is picked on the stage (the screen when
// nothing is); dragging drops it between the listed effects or onto a
// doll on the stage.
class CompLibrary final : public QWidget {
  Q_OBJECT

 public:
  explicit CompLibrary(const Managers& managers);

  QSize sizeHint() const override { return QSize(230, 320); }
  // A: the search field takes the keys.
  void FocusSearch();

 signals:
  void Problem(const QString& why);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void Report(const QString& problem) {
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }
  void Fill();
  // Lists the entries again, as the recently used changed.
  void Refill();
  void LoadRecent();
  void SaveRecent();
  // The words an entry is listed with: its name, then what it does.
  static std::pair<QString, QString> WordsOf(const CompEntry& entry);
  void AddItem(const QString& name, const QString& help,
               const CompEntry& entry, const QString& why_not);
  void AddHeader(const QString& name, const QString& help);
  // The entry an item adds; nothing for a header or a greyed one.
  std::optional<CompEntry> EntryAt(const QListWidgetItem* item) const;
  void StartDrag(const QListWidgetItem* item);
  // Hides the entries the search does not match.
  void Filter(const QString& text);
  void AddPicked(const QListWidgetItem* item);
  void ShowWhere();
  // Writes each group's effect count on its header.
  void CountGroups();
  void LoadFolds();
  // Folds the group under header shut, or opens it, and remembers it.
  void ToggleFold(QListWidgetItem* header);

  Managers managers_;
  QVBoxLayout layout_;
  QWidget body_;
  QVBoxLayout body_layout_;
  QLineEdit search_;
  QLabel where_;
  // Paints list_'s rows; outlives it.
  EffectRowDelegate delegate_;
  QListWidget list_;
  // Says when the search matches nothing.
  QLabel* none_ = nullptr;
  QListWidgetItem* pressed_ = nullptr;
  // The recently used as last listed.
  std::vector<CompEntry> listed_recent_;
  QPoint press_at_;
  // The groups folded shut, by name.
  QSet<QString> folded_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_LIBRARY_H_
