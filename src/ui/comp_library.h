#ifndef SNAPPER_UI_COMP_LIBRARY_H_
#define SNAPPER_UI_COMP_LIBRARY_H_

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

// The Comp tab's left side: every effect Snapper has, its own kinds,
// the Scene gradient and F's Plugins, under a search field. Hovering
// one shows what it does; a double click or Enter adds it on top of the
// stack, on what is picked on the stage (the screen when nothing is).
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
  void AddEntry(const QString& name, const QString& help, int kind,
                const QString& plugin, const QString& why_not);
  // Hides the entries the search does not match.
  void Filter(const QString& text);
  void AddPicked(const QListWidgetItem* item);
  void ShowWhere();

  Managers managers_;
  QVBoxLayout layout_;
  QWidget body_;
  QVBoxLayout body_layout_;
  QLineEdit search_;
  QLabel where_;
  QListWidget list_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_LIBRARY_H_
