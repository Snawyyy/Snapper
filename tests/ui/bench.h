#ifndef SNAPPER_TESTS_UI_BENCH_H_
#define SNAPPER_TESTS_UI_BENCH_H_

#include <QDir>
#include <QTemporaryDir>

#include "edit/dial_manager.h"
#include "edit/doll_library_manager.h"
#include "edit/document_manager.h"
#include "edit/draw_manager.h"
#include "edit/export_manager.h"
#include "edit/group_manager.h"
#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/link_manager.h"
#include "edit/object_manager.h"
#include "edit/playback_manager.h"
#include "edit/poke_manager.h"
#include "edit/pose_manager.h"
#include "edit/preset_manager.h"
#include "edit/reel_manager.h"
#include "edit/rig_manager.h"
#include "edit/selection_manager.h"
#include "edit/settings_manager.h"
#include "edit/shot_manager.h"
#include "edit/skin_manager.h"
#include "edit/stage_manager.h"
#include "edit/timing_chart_manager.h"
#include "ui/managers.h"
#include "ui/motion.h"
#include "ui/toast_manager.h"

namespace snapper {

// Every manager, as the app builds them, over a scratch data folder.
struct Bench final {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  DocumentManager document{&history, dir.path()};
  DollLibraryManager library{&history, QDir(dir.path()).filePath("dolls")};
  RigManager rig{&history};
  ShotManager shots{&history};
  ReelManager reel{&history};
  StageManager stage{&history};
  LinkManager links{&history};
  SelectionManager selection{&history};
  PoseManager pose{&history};
  KeyManager keys{&history};
  PresetManager presets{&history, QDir(dir.path()).filePath("poses.json")};
  PlaybackManager playback{&history};
  ExportManager exporter{&history, &playback};
  MotionManager motion{document.SettingsPath()};
  TimingChartManager timing{&history, &selection, &keys, &playback};
  ToastManager toasts{&history};
  PokeManager poke{&history, &playback};
  GroupManager groups{&history};
  SettingsManager settings{&history};
  DialManager dials{&history};
  DrawManager draw{&history, &playback, nullptr};
  SkinManager skins{&history};
  ObjectManager objects{&history, &library, &stage, &links};

  Managers All() {
    Managers all{&history, &document, &library, &rig, &shots, &stage,
                 &selection, &pose, &keys, &presets, &playback, &exporter,
                 &links, &reel, &motion, &toasts, &timing, &poke, &groups,
                 &settings};
    all.dials = &dials;
    all.draw = &draw;
    all.skins = &skins;
    all.objects = &objects;
    return all;
  }
};

// The piece 0 of the doll as layer 1 shows it: the layer's own warp dots
// (drag nodes, motions, reach) where it has changed them, else the doll's.
inline RigPiece Shown(const Bench& bench, const char* doll) {
  assert(doll != nullptr);
  assert(!bench.history.current().dolls.empty());
  return PieceFor(bench.history.current().dolls.at(doll)->rig.pieces[0],
                  LayerId(1));
}

}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_BENCH_H_
