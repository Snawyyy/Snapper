#include "app/app_context.h"

#include <cassert>

namespace snapper {

AppContext::AppContext() : history_(Project()), window_(&history_) {
  assert(!history_.IsDirty());
  assert(!history_.CanUndo());
}

}  // namespace snapper
