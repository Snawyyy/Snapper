#ifndef SNAPPER_UI_SHOT_CLIP_PANEL_H_
#define SNAPPER_UI_SHOT_CLIP_PANEL_H_

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "base/frame.h"
#include "model/reel.h"
#include "model/shot.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The Video tab's side panel for a picked shot clip, in the Pose box
// style: the shot's name and length (a new length resizes the shot as
// an edge drag would, ReelManager::ResizeShots), what a shot clip's
// edge drag does to the keys (Hold or Stretch, the Edges mode), and a
// button to open the shot on the Pose tab.
class ShotClipPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit ShotClipPanel(const Managers& managers);

  // Shows the picked shot clip; call when the pick or project changes.
  void Refresh();

 signals:
  void Problem(const QString& why);
  void OpenShotAsked(ShotId shot, Frame frame);
  // Hold or Stretch was picked.
  void EdgeModeChanged();

 private:
  // The one picked shot clip, or an invalid id.
  ClipId PickedClip() const;
  ShotId ShotOfClip(ClipId clip) const;
  void Rename();
  void Resize();
  void Open();
  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }

  Managers managers_;
  LiveEdit live_;
  QVBoxLayout layout_;
  QLineEdit name_;
  QSpinBox length_;
  QLabel seconds_;
  QPushButton hold_;
  QPushButton stretch_;
  QPushButton open_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_CLIP_PANEL_H_
