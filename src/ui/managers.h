#ifndef SNAPPER_UI_MANAGERS_H_
#define SNAPPER_UI_MANAGERS_H_

namespace snapper {

class CompManager;
class DialManager;
class DocumentManager;
class DrawManager;
class DollLibraryManager;
class ExportManager;
class GroupManager;
class HistoryManager;
class KeyManager;
class LinkManager;
class MotionManager;
class ObjectManager;
class LibraryFolderManager;
class PaletteManager;
class PlaybackManager;
class PokeManager;
class PoseManager;
class PresetManager;
class RecordManager;
class ReelManager;
class ReferenceManager;
class RigManager;
class SelectionManager;
class SettingsManager;
class ShotManager;
class SkinManager;
class SoundManager;
class StageManager;
class TimingChartManager;
class ToastManager;
class VoiceGenManager;
class VoiceLibrary;

// Every manager the widgets talk to. AppContext owns them; widgets only
// borrow, and each takes just the ones it uses.
struct Managers final {
  HistoryManager* history = nullptr;
  DocumentManager* document = nullptr;
  DollLibraryManager* library = nullptr;
  RigManager* rig = nullptr;
  ShotManager* shots = nullptr;
  StageManager* stage = nullptr;
  SelectionManager* selection = nullptr;
  PoseManager* pose = nullptr;
  KeyManager* keys = nullptr;
  PresetManager* presets = nullptr;
  PlaybackManager* playback = nullptr;
  ExportManager* exporter = nullptr;
  LinkManager* links = nullptr;
  ReelManager* reel = nullptr;
  MotionManager* motion = nullptr;
  ToastManager* toasts = nullptr;
  TimingChartManager* timing = nullptr;
  PokeManager* poke = nullptr;
  GroupManager* groups = nullptr;
  SettingsManager* settings = nullptr;
  // Not in IsComplete: widgets that show the palette assert it themselves.
  PaletteManager* palette = nullptr;
  // Not in IsComplete either: the smart dial box asserts it.
  DialManager* dials = nullptr;
  // Not in IsComplete either: draw mode's canvas and entries check it.
  DrawManager* draw = nullptr;
  // Not in IsComplete either: the skin boxes check it.
  SkinManager* skins = nullptr;
  // Not in IsComplete either: the object row, box and menu check it.
  ObjectManager* objects = nullptr;
  // Not in IsComplete either: the reel's waveforms and sound headers
  // check it.
  SoundManager* sounds = nullptr;
  // Not in IsComplete either: the stage, the Pose timeline's reference
  // rows, the References box and the shot menu check it.
  ReferenceManager* references = nullptr;
  // Not in IsComplete either: the Comp tab checks it.
  CompManager* comp = nullptr;
  // Not in IsComplete either: the Video tab's Record button and the sound
  // track headers' arm buttons check it.
  RecordManager* record = nullptr;
  // Not in IsComplete either: the Cast panel's library browser and the
  // View menu's Show hidden folders check it.
  LibraryFolderManager* library_folders = nullptr;
  // Not in IsComplete either: the Video tab's Voice panel checks them.
  VoiceLibrary* voices = nullptr;
  VoiceGenManager* voice_gen = nullptr;

  bool IsComplete() const {
    return history && document && library && rig && shots && stage &&
           selection && pose && keys && presets && playback && exporter &&
           links && reel && motion && toasts && timing && poke && groups &&
           settings;
  }
};

}  // namespace snapper

#endif  // SNAPPER_UI_MANAGERS_H_
