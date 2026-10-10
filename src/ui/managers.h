#ifndef SNAPPER_UI_MANAGERS_H_
#define SNAPPER_UI_MANAGERS_H_

namespace snapper {

class DocumentManager;
class DollLibraryManager;
class ExportManager;
class HistoryManager;
class KeyManager;
class LinkManager;
class MotionManager;
class PlaybackManager;
class PokeManager;
class PoseManager;
class PresetManager;
class ReelManager;
class RigManager;
class SelectionManager;
class ShotManager;
class StageManager;
class TimingChartManager;
class ToastManager;

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

  bool IsComplete() const {
    return history && document && library && rig && shots && stage &&
           selection && pose && keys && presets && playback && exporter &&
           links && reel && motion && toasts && timing && poke;
  }
};

}  // namespace snapper

#endif  // SNAPPER_UI_MANAGERS_H_
