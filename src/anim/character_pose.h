#ifndef SNAPPER_ANIM_CHARACTER_POSE_H_
#define SNAPPER_ANIM_CHARACTER_POSE_H_

#include <QPointF>
#include <QSizeF>
#include <QTransform>

#include <map>
#include <vector>

#include "anim/sampler.h"
#include "base/frame.h"
#include "model/character.h"
#include "model/layer.h"

namespace snapper {

// pose with delta added on top: moves, turns and orders add, scales and
// opacity multiply, a pick in delta wins. Used for smart bone actions.
CharacterPose AddPose(const CharacterPose& pose, const CharacterPose& delta);

// What smart bone's action adds at turn degrees: the blend of the two
// stops around it (the bone at rest is an empty stop), past the last
// stop it holds. A pick swaps halfway between stops.
CharacterPose ActionAt(const SmartAction& action, double turn);

// hand with every smart bone's action added, in the character's action
// order. A smart bone's own turn is kept (it drives, it bends nothing).
CharacterPose WithActions(const Character& character,
                          const CharacterPose& hand);

// The pose layer shows at frame: its keys sampled, actions added.
CharacterPose PoseAt(const Character& character, const CharacterLayer& layer,
                     Frame frame);

// A bone as posed, in character space.
struct PosedBone final {
  BoneId id{};
  QPointF base{};
  QPointF tip{};
  // Degrees, clockwise on screen.
  double angle = 0.0;
  bool is_smart = false;
  bool operator==(const PosedBone&) const = default;
};

// A part as posed, ready to draw. Only shown parts are listed.
struct PosedPart final {
  PartId id{};
  PartKind kind = PartKind::kGroup;
  // From the part's own space to character space (images, picking).
  QTransform to_character{};
  double opacity = 1.0;
  // kVector: each drawing point's place in character space, in the
  // drawing's point order.
  std::vector<QPointF> points{};
};

// A character at one pose: parts in draw order (back first) and bones.
struct PosedCharacter final {
  std::vector<PosedPart> parts{};
  std::vector<PosedBone> bones{};
  // Every part's own space to character space, shown or not; for a bone
  // part, the space its bones are in.
  std::map<PartId, QTransform> spaces{};
};

// Poses character: walks the tree with each part's rest and pose,
// shows one child of each switch, bends vector points by the bones of
// their bone part (flexi, by point or riding one bone) and lists the
// shown parts back to front. pose should already have WithActions.
PosedCharacter PoseCharacter(const Character& character,
                             const CharacterPose& pose);

// Only the bones of PoseCharacter, without bending any points.
std::vector<PosedBone> PoseBones(const Character& character,
                                 const CharacterPose& pose);

// How much each bone of bone_part pulls a point at rest place at (in the
// bone part's space) under flexi binding, summing to 1; smart bones pull
// none. A point out of every bone's reach follows the nearest bone.
std::vector<double> FlexiWeights(const CharacterPart& bone_part, QPointF at);

}  // namespace snapper

#endif  // SNAPPER_ANIM_CHARACTER_POSE_H_
