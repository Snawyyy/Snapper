#include "app/app_context.h"

#include <QDir>

#include <cassert>

namespace snapper {

AppContext::AppContext(const QString& data_folder)
    : history_(Project()),
      document_(&history_, data_folder),
      library_(&history_, QDir(data_folder).filePath("dolls")),
      rig_(&history_),
      shots_(&history_),
      stage_(&history_),
      selection_(&history_),
      pose_(&history_),
      keys_(&history_),
      presets_(&history_, QDir(data_folder).filePath("poses.json")),
      playback_(&history_),
      exporter_(&history_, &playback_),
      window_(&history_) {
  assert(!data_folder.isEmpty());
  assert(!history_.IsDirty());
}

AppContext::~AppContext() {
  assert(!history_.IsScopeOpen());
  // A clean exit leaves nothing to recover; unsaved work keeps its
  // autosave so it can be recovered next time.
  const bool is_saved = !history_.IsDirty();
  if (is_saved) {
    document_.DiscardAutosave();
  }
  assert(IsValidCanvas(history_.current().canvas));
}

}  // namespace snapper
