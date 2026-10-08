#ifndef SNAPPER_APP_APP_CONTEXT_H_
#define SNAPPER_APP_APP_CONTEXT_H_

#include <QString>
#include <QtClassHelperMacros>

#include "edit/doll_library_manager.h"
#include "edit/document_manager.h"
#include "edit/export_manager.h"
#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/preset_manager.h"
#include "edit/reel_manager.h"
#include "edit/rig_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "edit/stage_manager.h"
#include "ui/main_window.h"

namespace snapper {

// The composition root: builds every manager once, lowest first, and
// hands each one only what it needs. Members are destroyed in reverse,
// so a manager never outlives what it points at.
class AppContext final {
 public:
  // data_folder holds the doll library, saved poses, autosave and
  // settings.
  explicit AppContext(const QString& data_folder);
  ~AppContext();
  Q_DISABLE_COPY_MOVE(AppContext)

  MainWindow* window() { return &window_; }

 private:
  HistoryManager history_;
  DocumentManager document_;
  DollLibraryManager library_;
  RigManager rig_;
  ShotManager shots_;
  ReelManager reel_;
  StageManager stage_;
  SelectionManager selection_;
  PoseManager pose_;
  KeyManager keys_;
  PresetManager presets_;
  PlaybackManager playback_;
  ExportManager exporter_;
  MainWindow window_;
};

}  // namespace snapper

#endif  // SNAPPER_APP_APP_CONTEXT_H_
