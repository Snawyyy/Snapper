#ifndef SNAPPER_UI_MANAGERS_H_
#define SNAPPER_UI_MANAGERS_H_

namespace snapper {

class CharacterManager;
class CharacterPoseManager;
class CompManager;
class DocumentManager;
class StoryboardManager;
class DrawManager;
class ExportManager;
class HistoryManager;
class KeyManager;
class LinkManager;
class MotionManager;
class PaletteManager;
class PlaybackManager;
class PoseManager;
class RecordManager;
class ReelManager;
class ReferenceManager;
class SelectionManager;
class SettingsManager;
class ShotManager;
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
  CharacterManager* characters = nullptr;
  CharacterPoseManager* character_poses = nullptr;
  ShotManager* shots = nullptr;
  StageManager* stage = nullptr;
  SelectionManager* selection = nullptr;
  PoseManager* pose = nullptr;
  KeyManager* keys = nullptr;
  PlaybackManager* playback = nullptr;
  ExportManager* exporter = nullptr;
  LinkManager* links = nullptr;
  ReelManager* reel = nullptr;
  MotionManager* motion = nullptr;
  ToastManager* toasts = nullptr;
  TimingChartManager* timing = nullptr;
  SettingsManager* settings = nullptr;
  // Not in IsComplete: widgets that show the palette assert it themselves.
  PaletteManager* palette = nullptr;
  // Not in IsComplete either: draw mode's canvas and entries check it.
  DrawManager* draw = nullptr;
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
  // Not in IsComplete either: the Video tab's Voice panel checks them.
  VoiceLibrary* voices = nullptr;
  VoiceGenManager* voice_gen = nullptr;
  // Not in IsComplete either: the storyboard rows, box and keys check
  // it.
  StoryboardManager* storyboard = nullptr;

  bool IsComplete() const {
    return history && document && characters && character_poses &&
           shots && stage && selection && pose && keys && playback &&
           exporter && links && reel && motion && toasts && timing &&
           settings;
  }
};

}  // namespace snapper

#endif  // SNAPPER_UI_MANAGERS_H_
