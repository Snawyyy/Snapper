#include "edit/preset_manager.h"

#include <cassert>

#include "edit/history_manager.h"

namespace snapper {

PresetManager::PresetManager(HistoryManager* history, QString poses_path)
    : history_(history), poses_path_(std::move(poses_path)) {
  assert(history_ != nullptr);
  assert(!poses_path_.isEmpty());
  auto read = ReadPoses(poses_path_);
  if (read) {
    poses_ = std::move(*read);
  } else {
    // Kept, never overwritten: saving refuses until the file is fixed.
    load_error_ = read.error().message;
  }
}

}  // namespace snapper
