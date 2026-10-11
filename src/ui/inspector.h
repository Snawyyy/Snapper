#ifndef SNAPPER_UI_INSPECTOR_H_
#define SNAPPER_UI_INSPECTOR_H_

#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>
#include <vector>

#include "ui/drag_box.h"
#include "ui/fold_section.h"
#include "ui/shot_box.h"
#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/link_box.h"
#include "ui/managers.h"
#include "ui/point_motion_box.h"
#include "ui/poses_box.h"

namespace snapper {

// The right-hand panel: only the settings of what is picked. With
// nothing picked on the stage: the shot and its camera. With pieces or
// layers: their pose at the playhead, what they are
// linked to, the layer's settings, saved poses for a doll, and the
// picked warp dot's drag and animation.
// Stacked and scrollable.
class Inspector final : public QScrollArea {
  Q_OBJECT

 public:
  explicit Inspector(const Managers& managers);
  ~Inspector() override;

  // Shows the boxes for what is picked now.
  void Refresh();
  // The top line: a Teto red dot and what is picked.
  const QLabel& title() const { return title_; }

 signals:
  void Problem(const QString& why);

 private:
  // Puts box in the stack under a heading that folds it away (the
  // heading is box's title; the fold is kept in the settings, open at
  // first).
  void AddFolded(QGroupBox* box, const QString& key);
  // Shows or hides box with its heading.
  void Show(QGroupBox* box, bool is_shown);

  // The body holds the boxes; it comes first so they leave it first.
  QWidget body_;
  QVBoxLayout layout_;
  Managers managers_;
  // Each folded box and its heading, owned by the body.
  std::vector<std::pair<QGroupBox*, FoldSection*>> folds_;
  QLabel title_;
  QLabel hint_;
  ShotBox shot_;
  PoseBox pose_;
  DragBox drag_;
  PointMotionBox point_motion_;
  CameraBox camera_;
  LayerBox layer_;
  LinkBox link_;
  PosesBox poses_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_H_
