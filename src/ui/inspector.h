#ifndef SNAPPER_UI_INSPECTOR_H_
#define SNAPPER_UI_INSPECTOR_H_

#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <utility>
#include <vector>

#include "ui/dial_box.h"
#include "ui/drag_box.h"
#include "ui/fold_section.h"
#include "ui/shot_box.h"
#include "ui/keyed_boxes.h"
#include "ui/layer_box.h"
#include "ui/link_box.h"
#include "ui/managers.h"
#include "ui/object_box.h"
#include "ui/point_motion_box.h"
#include "ui/poses_box.h"
#include "ui/reference_box.h"
#include "ui/skin_box.h"
#include "ui/storyboard_box.h"

namespace snapper {

// The right-hand panel: only the settings of what is picked (see
// ShowFor). With nothing picked on the stage: the shot, and its camera
// folded closed at first. With pieces or layers: their pose at the
// playhead, what they are linked to, the layer's settings, a doll's
// dials (or a "+" tile for its first), skins when it has some, its saved
// poses folded, and the picked warp dot's drag and animation. A picked
// reference shows its settings. Stacked and scrollable; never wider
// than its share of the screen (KeepWideEnough).
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
  // first unless is_open_at_first is false).
  void AddFolded(QGroupBox* box, const QString& key,
                 bool is_open_at_first = true);
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
  DialBox dials_;
  // A doll with no dials: one tile whose menu makes the first.
  QToolButton new_dial_;
  SkinBox skins_;
  DragBox drag_;
  PointMotionBox point_motion_;
  CameraBox camera_;
  LayerBox layer_;
  LinkBox link_;
  PosesBox poses_;
  // Null without the object manager (some test benches).
  std::unique_ptr<ObjectBox> object_;
  // Null without the reference manager.
  std::unique_ptr<ReferenceBox> references_;
  // Null without the storyboard manager.
  std::unique_ptr<StoryboardBox> storyboard_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_INSPECTOR_H_
