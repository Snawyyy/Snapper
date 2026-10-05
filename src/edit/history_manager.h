#ifndef SNAPPER_EDIT_HISTORY_MANAGER_H_
#define SNAPPER_EDIT_HISTORY_MANAGER_H_

#include <QObject>
#include <QString>

#include <deque>

#include "model/project.h"

namespace snapper {

// Past this, the oldest steps are dropped.
constexpr int kMaxUndoSteps = 500;

// Owns the current project and its undo history. Every edit in Snapper
// ends in Commit here, so undo, redo, dirty state and repaint work the
// same way for every feature.
class HistoryManager final : public QObject {
  Q_OBJECT

 public:
  explicit HistoryManager(Project start);

  const Project& current() const { return current_; }

  // Makes next the project as one undo step named label ("Rotate
  // head"). An edit that changes nothing records nothing.
  void Commit(const QString& label, Project next);
  void Undo();
  void Redo();
  // Starts over with start, as opening a file does: no undo across it.
  void Reset(Project start);

  // False while a drag is open, so undo never cuts into one.
  bool CanUndo() const;
  bool CanRedo() const;
  // What Undo or Redo would undo or redo, empty when it can't.
  QString UndoLabel() const;
  QString RedoLabel() const;
  // Why Undo or Redo can't act, for the greyed-out button's tooltip;
  // empty when it can.
  QString WhyNoUndo() const;
  QString WhyNoRedo() const;

  // Dirty means the project differs from the last save. Undoing back
  // to the saved state makes it clean again.
  bool IsDirty() const { return revision_ != saved_revision_; }
  void MarkSaved();
  bool IsScopeOpen() const { return is_scope_open_; }

 signals:
  // The project, or what undo, redo or dirty would say, changed.
  void Changed();

 private:
  friend class EditScope;

  struct Step final {
    QString label;
    Project project;
    int revision = 0;
  };

  // EditScope's half: a drag shows live without making steps, then
  // lands as one step, or is put back.
  void OpenScope();
  void PreviewScope(Project next);
  void CloseScope(const QString& label, bool keep);

  void Record(const QString& label, Project before, int before_revision);
  void Travel(std::deque<Step>* from, std::deque<Step>* to);

  Project current_;
  std::deque<Step> undo_;
  std::deque<Step> redo_;
  // Each distinct state gets a revision, so dirty survives undo.
  int revision_ = 0;
  int next_revision_ = 1;
  int saved_revision_ = 0;

  bool is_scope_open_ = false;
  Project scope_start_;
  int scope_start_revision_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_HISTORY_MANAGER_H_
