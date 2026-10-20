#include "app/app_context.h"

#include <QDir>

#include <cassert>

namespace snapper {

AppContext::AppContext(const QString& data_folder)
    : history_(Project()),
      document_(&history_, data_folder),
      shots_(&history_),
      reel_(&history_),
      stage_(&history_),
      links_(&history_),
      selection_(&history_),
      pose_(&history_),
      keys_(&history_),
      playback_(&history_),
      exporter_(&history_, &playback_),
      motion_(document_.SettingsPath()),
      timing_(&history_, &selection_, &keys_, &playback_),
      toasts_(&history_),
      settings_(&history_),
      palette_(&history_, &selection_),
      draw_(&history_, &playback_, &palette_),
      sounds_(&history_),
      references_(&history_, &selection_),
      comp_(&history_),
      record_(&history_, &document_, &playback_, &reel_, data_folder),
      voices_(QDir(data_folder).filePath("voices")),
      voice_gen_(&document_, &playback_, &reel_, &voices_, data_folder),
      storyboard_(&history_, &document_, &draw_, &selection_, data_folder),
      characters_(&history_, QDir(data_folder).filePath("characters")),
      character_poses_(&history_),
      achievements_(&history_, &exporter_, document_.SettingsPath()),
      window_(Managers{.history = &history_,
                       .document = &document_,
                       .characters = &characters_,
                       .character_poses = &character_poses_,
                       .shots = &shots_,
                       .stage = &stage_,
                       .selection = &selection_,
                       .pose = &pose_,
                       .keys = &keys_,
                       .playback = &playback_,
                       .exporter = &exporter_,
                       .links = &links_,
                       .reel = &reel_,
                       .motion = &motion_,
                       .toasts = &toasts_,
                       .timing = &timing_,
                       .settings = &settings_,
                       .palette = &palette_,
                       .draw = &draw_,
                       .sounds = &sounds_,
                       .references = &references_,
                       .comp = &comp_,
                       .record = &record_,
                       .voices = &voices_,
                       .voice_gen = &voice_gen_,
                       .storyboard = &storyboard_,
                       .achievements = &achievements_}) {
  assert(!data_folder.isEmpty());
  // Declared after playback, so the reel's sound is handed over here.
  playback_.SetSounds(&sounds_);
  assert(!history_.IsDirty());
}

AppContext::~AppContext() {
  assert(!history_.IsScopeOpen());
  // The sounds go before playback does.
  playback_.Pause();
  playback_.SetSounds(nullptr);
  // A clean exit leaves nothing to recover; unsaved work keeps its
  // autosave so it can be recovered next time.
  const bool is_saved = !history_.IsDirty();
  if (is_saved) {
    document_.DiscardAutosave();
  }
  assert(IsValidCanvas(history_.current().canvas));
}

}  // namespace snapper
