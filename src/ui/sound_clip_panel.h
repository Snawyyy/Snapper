#ifndef SNAPPER_UI_SOUND_CLIP_PANEL_H_
#define SNAPPER_UI_SOUND_CLIP_PANEL_H_

#include <QDoubleSpinBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

#include "model/reel.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The Video tab's side panel for picked sound clips, in the Pose box
// style: a plated Sound clip section with the file's name and length,
// then faders for the volume (100 % is as recorded, up to +6 dB) and
// the fades in and out (seconds). Every picked sound clip takes the
// change, as one undo step per drag or typed value.
class SoundClipPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit SoundClipPanel(const Managers& managers);

  // Shows the picked sound clips; call when the pick or project changes.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  std::vector<ClipId> Picked() const;
  void Commit(const std::vector<ClipId>& clips, int what);
  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }

  Managers managers_;
  LiveEdit live_;
  QVBoxLayout layout_;
  QLabel name_;
  QDoubleSpinBox volume_;
  QDoubleSpinBox fade_in_;
  QDoubleSpinBox fade_out_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SOUND_CLIP_PANEL_H_
