#ifndef SNAPPER_EDIT_EDIT_SCOPE_H_
#define SNAPPER_EDIT_EDIT_SCOPE_H_

#include <QString>
#include <QtClassHelperMacros>

#include "model/project.h"

namespace snapper {

class HistoryManager;

// One drag, one undo step. Opening the scope freezes undo; each
// Preview shows the drag live; leaving the scope lands it as a single
// step named label, or puts the start back after Cancel. Only one
// scope is open at a time.
class EditScope final {
 public:
  EditScope(HistoryManager* history, QString label);
  ~EditScope();
  Q_DISABLE_COPY_MOVE(EditScope)

  void Preview(Project next);
  // Escape: the drag never happened.
  void Cancel() { is_cancelled_ = true; }

 private:
  HistoryManager* history_;
  QString label_;
  bool is_cancelled_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_EDIT_SCOPE_H_
