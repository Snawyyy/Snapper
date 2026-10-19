#ifndef SNAPPER_UI_CHARACTER_CANVAS_H_
#define SNAPPER_UI_CHARACTER_CANVAS_H_

#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QWidget>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "anim/character_pose.h"
#include "edit/edit_scope.h"
#include "render/image_cache.h"
#include "ui/character_session.h"
#include "ui/view_camera.h"

namespace snapper {

class CharacterManager;
class HistoryManager;

// The Character tab's stage: the character at rest (or posed on a smart
// bone's stop), its picked part's points and curves, its bones, and the
// tools of CharacterTool working on them. Every change goes through
// CharacterManager; a drag is one undo step and Escape cancels it.
class CharacterCanvas final : public QWidget {
  Q_OBJECT

 public:
  CharacterCanvas(HistoryManager* history, CharacterManager* characters,
                  CharacterSession* session, QWidget* parent = nullptr);

  // The character on show, or nullptr.
  const Character* Shown() const;
  // The pose the canvas shows: rest, a bone being tried, or the smart
  // bone's stop being posed (actions included).
  CharacterPose ShownPose() const;
  // Frames the whole character.
  void FitAll();

 signals:
  // A refusal to show the user, or empty when the last act worked.
  void Problem(const QString& message);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  // What a press started.
  enum class Drag {
    kNone,
    kPoints,
    kBox,
    kBoneTip,
    kBoneWhole,
    kPart,
    kStroke,
    kBoxShape,
    kAddLine,
    kAddBone,
    kCurvature,
    kStrength,
    kTry,
  };

  // Character space to widget pixels, and its scale.
  QTransform View() const;
  double Scale() const;
  QPointF ToCharacter(QPointF widget) const;
  // The posed character for ShownPose.
  PosedCharacter Posed() const;
  // The picked vector part as shown, or nullptr.
  const PosedPart* PickedShown(const PosedCharacter& posed) const;
  // The bone part whose bones show: the picked part if it is one, else
  // the bone part around it.
  const CharacterPart* BonesShown() const;
  // A widget-pixel step turned into the picked part's own space.
  QPointF IntoPart(PartId part, QPointF from, QPointF to) const;
  // The same into a bone part's space (for bone rest edits).
  QPointF IntoBones(const CharacterPart& bones, QPointF widget) const;

  void PaintGrid(QPainter* painter) const;
  void PaintPicked(QPainter* painter, const PosedCharacter& posed) const;
  void PaintBones(QPainter* painter, const PosedCharacter& posed) const;
  void PaintTool(QPainter* painter) const;

  // The point of the picked part under widget position at, or none.
  PointId PointAt(const PosedCharacter& posed, QPointF at) const;
  // The bone under at: its id and whether the tip was grabbed.
  std::optional<std::pair<BoneId, bool>> BoneAt(const PosedCharacter& posed,
                                                QPointF at) const;

  void PressPick(QMouseEvent* event);
  void PressAdd(QMouseEvent* event);
  void PressBind(QMouseEvent* event);
  void MoveDrag(QPointF at, Qt::KeyboardModifiers mods);
  void FinishDrag(QPointF at, Qt::KeyboardModifiers mods);
  void FinishStroke();
  void FinishBoxShape(QPointF at, bool is_even);
  void FinishAddLine(QPointF at);
  void FinishAddBone(QPointF at);
  void Fill(QPointF at);
  // Picks the front-most part under at, if any.
  bool PickPartAt(QPointF at);
  void DeletePicked();
  void Report(const QString& problem);
  // Posing a smart bone's stop: pose change goes into the stop.
  bool IsPosingStop() const;
  void PoseStop(const CharacterPose& pose);
  void OpenScope(const QString& label);
  void CloseScope(bool keep);

  HistoryManager* history_;
  CharacterManager* characters_;
  CharacterSession* session_;
  ViewCamera camera_;
  mutable ImageCache cache_;
  std::unique_ptr<EditScope> scope_;
  Drag drag_ = Drag::kNone;
  QPointF press_;
  QPointF last_;
  QPointF now_;
  std::vector<QPointF> stroke_;
  // kAddLine: the point the line starts from, if it starts on one.
  PointId from_point_;
  // kCurvature and kStrength: what the drag started from.
  double start_value_ = 0.0;
  BoneId drag_bone_;
  // kTryBones without a stop: the pose being tried, dropped on release.
  std::optional<CharacterPose> trying_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_CANVAS_H_
