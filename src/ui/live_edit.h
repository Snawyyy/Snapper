#ifndef SNAPPER_UI_LIVE_EDIT_H_
#define SNAPPER_UI_LIVE_EDIT_H_

#include <QAbstractSpinBox>
#include <QObject>
#include <QSignalBlocker>
#include <QString>

#include <cassert>
#include <memory>

#include "edit/edit_scope.h"
#include "edit/history_manager.h"

namespace snapper {

// Lets a number field change the project as it is typed or scrolled,
// while one burst of changes (until Enter or leaving the field) stays a
// single undo step.
class LiveEdit final {
 public:
  explicit LiveEdit(HistoryManager* history) : history_(history) {
    assert(history_ != nullptr);
  }

  // Opens the step for a burst, unless one (or a drag) is already open.
  void Touch(const QString& label) {
    assert(!label.isEmpty());
    const bool can_open = scope_ == nullptr && !history_->IsScopeOpen();
    if (can_open) {
      scope_ = std::make_unique<EditScope>(history_, label);
    }
  }
  // Lands the burst as one step.
  void Finish() { scope_.reset(); }

 private:
  HistoryManager* history_;
  std::unique_ptr<EditScope> scope_;
};

// Applies box live: every value change calls commit inside one undo step
// named label, closed when editing finishes. Programmatic changes must
// block the box's signals (ShowNumber does).
template <typename Box, typename Commit>
void MakeLive(Box* box, LiveEdit* live, const QString& label,
              QObject* owner, Commit commit) {
  assert(box != nullptr && live != nullptr);
  assert(owner != nullptr);
  box->setKeyboardTracking(true);
  QObject::connect(box, &Box::valueChanged, owner,
                   [box, live, label, commit] {
                     // Opening the step repaints the panels, which would
                     // put the old number back; keep the typed one.
                     const auto typed = box->value();
                     live->Touch(label);
                     const QSignalBlocker quiet(box);
                     box->setValue(typed);
                     commit();
                   });
  QObject::connect(box, &QAbstractSpinBox::editingFinished, owner,
                   [live] { live->Finish(); });
}

}  // namespace snapper

#endif  // SNAPPER_UI_LIVE_EDIT_H_
