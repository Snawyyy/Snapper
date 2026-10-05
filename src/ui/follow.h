#ifndef SNAPPER_UI_FOLLOW_H_
#define SNAPPER_UI_FOLLOW_H_

#include <QObject>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/managers.h"

namespace snapper {

// Calls box->Refresh() whenever what an inspector box shows may have
// changed: an edit, a new pick, or the playhead moving.
template <typename Box>
void Follow(const Managers& managers, Box* box) {
  assert(box != nullptr);
  assert(managers.IsComplete());
  const auto refresh = [box] { box->Refresh(); };
  QObject::connect(managers.history, &HistoryManager::Changed, box, refresh);
  QObject::connect(managers.selection, &SelectionManager::Changed, box,
                   refresh);
  QObject::connect(managers.playback, &PlaybackManager::FrameChanged, box,
                   refresh);
}

}  // namespace snapper

#endif  // SNAPPER_UI_FOLLOW_H_
