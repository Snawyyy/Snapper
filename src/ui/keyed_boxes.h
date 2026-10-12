#ifndef SNAPPER_UI_KEYED_BOXES_H_
#define SNAPPER_UI_KEYED_BOXES_H_

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>

#include <array>
#include <vector>

#include "base/frame.h"
#include "edit/track_ref.h"
#include "model/project.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// Exact numbers for the picked piece or layer at the playhead. Typing a
// value keys it there, like dragging on the stage does.
class PoseBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit PoseBox(const Managers& managers);
  // Shows the picked track's pose at the playhead.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  enum Field {
    kTurn, kX, kY, kScaleX, kScaleY, kSkew, kOpacity, kLean, kSwivel,
    kCount
  };

  void Commit();
  void SwapDrawing(int index);
  void ShowPose(const Project& project, const std::vector<TrackRef>& tracks,
                Frame frame, const PiecePose& focus);

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  std::array<QDoubleSpinBox, kCount> fields_;
  // The Lean and Swivel dials' row, inside layout_; for a whole doll only.
  QWidget* turns_row_ = nullptr;
  QComboBox drawing_;
  // How many of the piece's drawings the picker lists, after "Rig
  // default"; the new drawing entries follow them.
  int drawing_count_ = 0;
};

// The camera of the shot under the playhead, keyed at the playhead.
class CameraBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit CameraBox(const Managers& managers);
  // Shows the camera at the playhead.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  enum Field { kX, kY, kZoom, kTurn, kShake, kCount };

  void Commit();

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  std::array<QDoubleSpinBox, kCount> fields_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_KEYED_BOXES_H_
