#ifndef SNAPPER_EDIT_CHANGE_WATCHER_H_
#define SNAPPER_EDIT_CHANGE_WATCHER_H_

#include <QObject>

#include "model/project.h"

namespace snapper {

class HistoryManager;

// Tells a view what the project was before the step that just landed,
// so it can diff the two (change_diff.h) and animate the answer. It
// only reads: the project has changed by the time anyone hears of it.
//
// A drag is one step, heard when it lands, not on every move. A new
// file, or a cancelled drag, is no step. A step that goes back to a
// state the history has already been in is undo or redo, which a view
// shows differently (a rewind) from a fresh edit.
class ChangeWatcher final : public QObject {
  Q_OBJECT

 public:
  ChangeWatcher(HistoryManager* history, QObject* parent);

 signals:
  // before is the project the step started from. is_travel is true for
  // undo and redo.
  void Stepped(const Project& before, bool is_travel);

 private:
  void OnChanged();

  HistoryManager* history_;
  // The project as of the last time this heard of one.
  Project seen_;
  // The newest state the history has shown this; anything not newer
  // than it is a state being travelled back to.
  int newest_revision_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHANGE_WATCHER_H_
