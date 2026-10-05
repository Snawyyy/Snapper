#ifndef SNAPPER_EDIT_PRESET_MANAGER_H_
#define SNAPPER_EDIT_PRESET_MANAGER_H_

#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

#include "anim/presets.h"
#include "base/error.h"
#include "base/frame.h"
#include "edit/track_ref.h"
#include "io/pose_file.h"

namespace snapper {

class HistoryManager;

// Ready-made motion: loops laid onto pieces or layers, and poses saved
// by name to reuse in any project. Saved poses live in their own file,
// outside the project, so saving one is not an undoable edit.
class PresetManager final : public QObject {
  Q_OBJECT

 public:
  PresetManager(HistoryManager* history, QString poses_path);

  // Lays the loop onto each layer or piece track from start, on top of
  // what is there, and keys the old pose back where the loop ends.
  Result<void> ApplyMotion(const std::vector<TrackRef>& tracks, Frame start,
                           const PresetSettings& settings);

  // Names of the saved poses, sorted.
  QStringList SavedPoses() const;
  // Saves the doll layer's whole pose at frame under name, replacing a
  // saved pose of that name.
  Result<void> SavePose(const QString& name, ShotId shot, LayerId layer,
                        Frame frame);
  // Keys a saved pose at frame on every piece the doll shares with it.
  Result<void> ApplyPose(const QString& name, ShotId shot, LayerId layer,
                         Frame frame);
  Result<void> DeletePose(const QString& name);
  // Why the saved poses couldn't be read; empty when they could. While
  // set, nothing is saved, so a damaged file is never overwritten.
  const QString& load_error() const { return load_error_; }

 signals:
  void SavedPosesChanged();

 private:
  Result<void> Store(std::vector<SavedPose> poses);

  HistoryManager* history_;
  QString poses_path_;
  std::vector<SavedPose> poses_;
  QString load_error_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_PRESET_MANAGER_H_
