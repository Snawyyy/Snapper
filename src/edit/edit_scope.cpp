#include "edit/edit_scope.h"

#include <cassert>
#include <utility>

#include "edit/history_manager.h"

namespace snapper {

EditScope::EditScope(HistoryManager* history, QString label)
    : history_(history), label_(std::move(label)) {
  assert(history_ != nullptr);
  assert(!label_.isEmpty());
  history_->OpenScope();
}

EditScope::~EditScope() {
  assert(history_ != nullptr);
  assert(history_->IsScopeOpen());
  history_->CloseScope(label_, !is_cancelled_);
}

void EditScope::Preview(Project next) {
  assert(history_ != nullptr);
  assert(!is_cancelled_);
  history_->PreviewScope(std::move(next));
}

}  // namespace snapper
