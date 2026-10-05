#ifndef SNAPPER_TESTS_UI_BENCH_H_
#define SNAPPER_TESTS_UI_BENCH_H_

#include <QDir>
#include <QTemporaryDir>

#include "edit/doll_library_manager.h"
#include "edit/document_manager.h"
#include "edit/export_manager.h"
#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/preset_manager.h"
#include "edit/rig_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "edit/stage_manager.h"
#include "ui/managers.h"

namespace snapper {

// Every manager, as the app builds them, over a scratch data folder.
struct Bench final {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document{&history, dir.path()};
  DollLibraryManager library{&history, QDir(dir.path()).filePath("dolls")};
  RigManager rig{&history};
  ShotManager shots{&history};
  StageManager stage{&history};
  SelectionManager selection{&history};
  PoseManager pose{&history};
  KeyManager keys{&history};
  PresetManager presets{&history, QDir(dir.path()).filePath("poses.json")};
  PlaybackManager playback{&history};
  ExportManager exporter{&history, &playback};

  Managers All() {
    return {&history, &document, &library, &rig, &shots, &stage,
            &selection, &pose, &keys, &presets, &playback, &exporter};
  }
};

}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_BENCH_H_
