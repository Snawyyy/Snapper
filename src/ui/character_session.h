#ifndef SNAPPER_UI_CHARACTER_SESSION_H_
#define SNAPPER_UI_CHARACTER_SESSION_H_

#include <QObject>
#include <QString>

#include <set>

#include "model/character.h"

namespace snapper {

// The Character tab's tools, as in Moho.
// - kPick: pick and drag points (vector part) or bones (bone part), box
//   pick on empty space.
// - kMovePart: drag the picked part's rest place, wheel turns it.
// - kAddPoint: click adds a point after the picked end, drag draws a
//   line; on a bone part it adds a bone from press to release.
// - kFreehand: draws a smooth line through the stroke.
// - kOval, kRectangle: drag a closed filled shape.
// - kCurvature: drag sideways to round or sharpen the picked points.
// - kFill: click a shape to give it the current colors.
// - kStrength: drag on a bone to set how far it pulls points.
// - kBindPoints: click points to tie them to the picked bone.
// - kTryBones: drag bones to try the rig out; let go and they spring
//   back (nothing is saved). On a smart bone stop, posing saves.
enum class CharacterTool {
  kPick,
  kMovePart,
  kAddPoint,
  kFreehand,
  kOval,
  kRectangle,
  kCurvature,
  kFill,
  kStrength,
  kBindPoints,
  kTryBones,
};
inline constexpr int kCharacterToolCount = 11;

// What the Character tab shows and has picked. View state only: every
// change to the character goes through CharacterManager.
class CharacterSession final : public QObject {
  Q_OBJECT

 public:
  const QString& character() const { return character_; }
  PartId part() const { return part_; }
  const std::set<PointId>& points() const { return points_; }
  BoneId bone() const { return bone_; }
  int shape() const { return shape_; }
  CharacterTool tool() const { return tool_; }
  // While a smart bone's stop is being posed: the bone and the stop.
  BoneId action_bone() const { return action_bone_; }
  int action_stop() const { return action_stop_; }
  // The smart bone's turn shown while trying or posing its action.
  double action_turn() const { return action_turn_; }
  // The colors and width new shapes take.
  const VectorShape& style() const { return style_; }

  void SetCharacter(const QString& character);
  void SetPart(PartId part);
  void SetPoints(std::set<PointId> points);
  void SetBone(BoneId bone);
  void SetShape(int shape);
  void SetTool(CharacterTool tool);
  void SetAction(BoneId bone, int stop, double turn);
  void SetStyle(const VectorShape& style);

 signals:
  void Changed();

 private:
  QString character_;
  PartId part_;
  std::set<PointId> points_;
  BoneId bone_;
  int shape_ = -1;
  CharacterTool tool_ = CharacterTool::kPick;
  BoneId action_bone_;
  int action_stop_ = -1;
  double action_turn_ = 0.0;
  VectorShape style_{};
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_SESSION_H_
