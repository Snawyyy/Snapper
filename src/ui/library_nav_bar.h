#ifndef SNAPPER_UI_LIBRARY_NAV_BAR_H_
#define SNAPPER_UI_LIBRARY_NAV_BAR_H_

#include <QHBoxLayout>
#include <QLineEdit>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

// The bar over the doll library, as in a file manager: Back, Forward and
// Up between the folders visited, an address you can read and type into
// (Enter opens it), and a button that makes a folder in the open one. A
// control that cannot act is greyed out with the reason as its tooltip.
class LibraryNavBar final : public QWidget {
  Q_OBJECT

 public:
  explicit LibraryNavBar(const Managers& managers);

 signals:
  void Problem(const QString& why);

 private:
  void Refresh();
  void OpenTyped();
  void Report(const QString& problem) {
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }

  Managers managers_;
  QHBoxLayout layout_;
  QToolButton back_;
  QToolButton forward_;
  QToolButton up_;
  QLineEdit address_;
  QToolButton new_folder_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LIBRARY_NAV_BAR_H_
