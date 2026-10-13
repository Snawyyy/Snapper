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
      reel_(&history_),
      stage_(&history_),
      links_(&history_),
      selection_(&history_),
      pose_(&history_),
      keys_(&history_),
      presets_(&history_, QDir(data_folder).filePath("poses.json")),
      playback_(&history_),
      exporter_(&history_, &playback_),
      motion_(document_.SettingsPath()),
      timing_(&history_, &selection_, &keys_, &playback_),
      toasts_(&history_),
      poke_(&history_, &playback_),
      groups_(&history_),
      settings_(&history_),
      palette_(&history_, &selection_),
      dials_(&history_),
      draw_(&history_, &playback_, &palette_),
      skins_(&history_),
      objects_(&history_, &library_, &stage_, &links_),
      sounds_(&history_),
      references_(&history_),
      comp_(&history_),
      record_(&history_, &document_, &playback_, &reel_, data_folder),
      window_(Managers{&history_, &document_, &library_, &rig_, &shots_,
                       &stage_, &selection_, &pose_, &keys_, &presets_,
                       &playback_, &exporter_, &links_, &reel_, &motion_,
                       &toasts_, &timing_, &poke_, &groups_, &settings_,
                       &palette_, &dials_, &draw_, &skins_, &objects_,
                       &sounds_, &references_, &comp_, &record_}) {
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
