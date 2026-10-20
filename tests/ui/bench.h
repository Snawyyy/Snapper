#ifndef SNAPPER_TESTS_UI_BENCH_H_
#define SNAPPER_TESTS_UI_BENCH_H_

#include <QDir>
#include <QTemporaryDir>

#include "edit/achievement_manager.h"
#include "edit/character_manager.h"
#include "edit/character_pose_manager.h"
#include "edit/comp_manager.h"
#include "edit/storyboard_manager.h"
#include "edit/document_manager.h"
#include "edit/draw_manager.h"
#include "edit/export_manager.h"
#include "edit/history_manager.h"
#include "edit/key_manager.h"
#include "edit/link_manager.h"
#include "edit/palette_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/record_manager.h"
#include "edit/reel_manager.h"
#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "edit/settings_manager.h"
#include "edit/shot_manager.h"
#include "edit/sound_manager.h"
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
  ShotManager shots{&history};
  ReelManager reel{&history};
  StageManager stage{&history};
  LinkManager links{&history};
  SelectionManager selection{&history};
  PoseManager pose{&history};
  KeyManager keys{&history};
  PlaybackManager playback{&history};
  ExportManager exporter{&history, &playback};
  MotionManager motion{document.SettingsPath()};
  TimingChartManager timing{&history, &selection, &keys, &playback};
  ToastManager toasts{&history};
  SettingsManager settings{&history};
  DrawManager draw{&history, &playback, nullptr};
  SoundManager sounds{&history};
  ReferenceManager references{&history};
  CompManager comp{&history};
  RecordManager record{&history, &document, &playback, &reel, dir.path()};
  StoryboardManager storyboard{&history, &document, &draw, &selection,
                              dir.path()};
  PaletteManager palette{&history, &selection};
  CharacterManager characters{&history,
                              QDir(dir.path()).filePath("characters")};
  CharacterPoseManager character_poses{&history};
  AchievementManager achievements{&history, &exporter,
                                  document.SettingsPath()};

  Managers All() {
    return Managers{.history = &history,
                    .document = &document,
                    .characters = &characters,
                    .character_poses = &character_poses,
                    .shots = &shots,
                    .stage = &stage,
                    .selection = &selection,
                    .pose = &pose,
                    .keys = &keys,
                    .playback = &playback,
                    .exporter = &exporter,
                    .links = &links,
                    .reel = &reel,
                    .motion = &motion,
                    .toasts = &toasts,
                    .timing = &timing,
                    .settings = &settings,
                    .palette = &palette,
                    .draw = &draw,
                    .sounds = &sounds,
                    .references = &references,
                    .comp = &comp,
                    .record = &record,
                    .storyboard = &storyboard,
                    .achievements = &achievements};
  }
};

}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_BENCH_H_
