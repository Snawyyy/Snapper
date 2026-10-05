#ifndef SNAPPER_APP_APP_CONTEXT_H_
#define SNAPPER_APP_APP_CONTEXT_H_

#include <QtClassHelperMacros>

#include "edit/history_manager.h"
#include "ui/main_window.h"

namespace snapper {

// The composition root: builds every manager once, lowest first, and
// hands each one only what it needs. Members are destroyed in reverse,
// so a manager never outlives what it points at.
class AppContext final {
 public:
  AppContext();
  Q_DISABLE_COPY_MOVE(AppContext)

  MainWindow* window() { return &window_; }

 private:
  HistoryManager history_;
  MainWindow window_;
};

}  // namespace snapper

#endif  // SNAPPER_APP_APP_CONTEXT_H_
