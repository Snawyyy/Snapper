#ifndef SNAPPER_MODEL_CHARACTER_H_
#define SNAPPER_MODEL_CHARACTER_H_

#include <QPointF>
#include <QSizeF>
#include <QString>

#include <map>
#include <vector>

#include "base/id.h"
#include "model/vector.h"

namespace snapper {

struct PartTag;
struct BoneTag;
// A layer of a character, unique inside it.
using PartId = Id<PartTag>;
// A bone of a character, unique inside it.
using BoneId = Id<BoneTag>;

constexpr int kMaxCharacterParts = 512;
constexpr int kMaxCharacterBones = 256;
constexpr int kMaxSmartActions = 128;
constexpr int kMaxCharacterStyles = 64;
constexpr int kMaxActionStops = 32;

// What a part of a character is, as in Moho's layer kinds.
// - kGroup: holds other parts and moves them as one.
// - kBone: a group with a skeleton that bends the parts inside it.
// - kVector: points, curves and shapes.
// - kSwitch: shows one of its children at a time, keyed.
// - kImage: one picture, a prop or a part drawn elsewhere.
enum class PartKind { kGroup, kBone, kVector, kSwitch, kImage };
inline constexpr int kPartKindCount = 5;

// True for the kinds that hold other parts.
bool IsFolder(PartKind kind);

// How a part's points follow the bones of the bone part around it.
// - kFlexi: each point follows the bones near it, blended by how far
//   it is and each bone's strength (Moho's flexi binding, the default).
// - kPoints: each point follows the bone it is bound to by hand; an
//   unbound point stays with the part.
// - kLayer: the whole part rides one bone.
// - kNone: bones do not move it.
enum class BindMode { kFlexi, kPoints, kLayer, kNone };
inline constexpr int kBindModeCount = 4;

// A bone at rest, in its bone part's space (pixels, y down). Angles are
// degrees, clockwise on screen; 0 points right.
struct Bone final {
  BoneId id{};
  QString name{};
  // None for a root bone.
  BoneId parent{};
  QPointF base{};
  double angle = 0.0;
  double length = 100.0;
  // How far, in pixels from the bone, flexi binding reaches.
  double strength = 60.0;
  // Turn limits in degrees from rest, when has_limits.
  bool has_limits = false;
  double min_turn = -90.0;
  double max_turn = 90.0;
  // A smart bone drives actions by its turn and moves no points itself.
  bool is_smart = false;
  // An IK drag does not turn it.
  bool is_locked = false;
  // Lags and bounces behind its parent (bone dynamics), 0 is off.
  double dynamics = 0.0;

  bool operator==(const Bone&) const = default;
};

// A part's place, turn and look on top of its rest (keyed). Point moves
// add to the drawing's own points, by point.
struct PartPose final {
  QPointF offset{};
  double rotation = 0.0;
  double scale_x = 1.0;
  double scale_y = 1.0;
  double opacity = 1.0;
  int order = 0;
  // A switch part's shown child; none keeps the rest pick.
  PartId pick{};
  std::map<PointId, QPointF> points{};

  bool operator==(const PartPose&) const = default;
};

// A bone's turn, push and stretch on top of its rest (keyed).
struct BonePose final {
  double turn = 0.0;
  QPointF offset{};
  double scale = 1.0;

  bool operator==(const BonePose&) const = default;
};

// A full pose of a character: what a key on its timeline row holds. A
// part or bone missing here sits at rest.
struct CharacterPose final {
  // The whole character's move on the stage.
  PartPose root{};
  std::map<PartId, PartPose> parts{};
  std::map<BoneId, BonePose> bones{};

  bool operator==(const CharacterPose&) const = default;
};

// One saved place of a smart bone's action: at a turn of the bone, the
// parts and bones are posed this much past their hand pose (moves and
// turns add, scales multiply, picks swap).
struct ActionStop final {
  // Degrees the smart bone is turned from rest.
  double turn = 0.0;
  CharacterPose pose{};

  bool operator==(const ActionStop&) const = default;
};

// What a smart bone does as it turns: a head turn, a blink, a mouth.
// Stops are sorted by turn; between two the pose blends, past the last
// it holds. A bone turned 0 is the rest stop, which is always empty.
struct SmartAction final {
  BoneId bone{};
  QString name{};
  std::vector<ActionStop> stops{};

  bool operator==(const SmartAction&) const = default;
};

// One part of a character at rest.
struct CharacterPart final {
  PartId id{};
  QString name{};
  PartKind kind = PartKind::kGroup;
  // None for a part at the top.
  PartId parent{};
  // The point it turns and scales around, in its parent's space.
  QPointF origin{};
  // Its rest place, turn and scale.
  QPointF offset{};
  double rotation = 0.0;
  double scale_x = 1.0;
  double scale_y = 1.0;
  double opacity = 1.0;
  bool is_visible = true;
  bool is_flipped = false;

  BindMode bind = BindMode::kFlexi;
  // kLayer: the bone it rides.
  BoneId bound_bone{};
  // kPoints: each bound point's bone.
  std::map<PointId, BoneId> point_bones{};

  // kVector.
  VectorDrawing drawing{};
  // kBone.
  std::vector<Bone> bones{};
  // kSwitch: the child shown at rest.
  PartId rest_pick{};
  // kImage: the picture's path inside the character folder and its size;
  // its top left sits at offset.
  QString image{};
  QSizeF image_size{};

  bool operator==(const CharacterPart&) const = default;
};

// A named look shapes can share, as Moho's styles: change the style and
// every shape using it changes. Only look's fill, line, width and brush
// count; its curves are unused.
struct VectorStyle final {
  StyleId id{};
  QString name{};
  VectorShape look{};

  bool operator==(const VectorStyle&) const = default;
};

// A character as the library and a project hold it: a tree of parts.
// Parts are listed bottom to top, so a child later in the list draws in
// front of its siblings earlier in it; a parent comes before its
// children.
struct Character final {
  QString name{};
  // The folder holding its pictures, if any.
  QString folder{};
  std::vector<CharacterPart> parts{};
  std::vector<SmartAction> actions{};
  std::vector<VectorStyle> styles{};
  // The next free id for a part, bone, point or style; ids are never
  // reused.
  int next_id = 1;

  bool operator==(const Character&) const = default;
};

// The style id, or nullptr.
const VectorStyle* FindStyle(const Character& character, StyleId id);
VectorStyle* FindStyle(Character* character, StyleId id);
// How shape looks: its style's look (with shape's curves) when it uses
// one that is there, else itself.
VectorShape LookOf(const Character& character, const VectorShape& shape);

// Where bone's tip is at rest, in its bone part's space.
QPointF TipOf(const Bone& bone);

const CharacterPart* FindPart(const Character& character, PartId id);
CharacterPart* FindPart(Character* character, PartId id);
// The part called name, or nullptr.
const CharacterPart* FindPart(const Character& character,
                              const QString& name);
// Index of part id in character.parts, or -1.
int PartIndex(const Character& character, PartId id);
// The bone id and the part holding it, or nullptrs.
const Bone* FindBone(const Character& character, BoneId id);
const CharacterPart* BoneOwner(const Character& character, BoneId id);
Bone* FindBone(Character* character, BoneId id);
// The action of smart bone, or nullptr.
const SmartAction* FindAction(const Character& character, BoneId bone);
SmartAction* FindAction(Character* character, BoneId bone);
// The parts whose parent is id (none: the top parts), bottom to top.
std::vector<PartId> ChildrenOf(const Character& character, PartId id);
// The bone part that bends part: its nearest bone ancestor, or nullptr.
const CharacterPart* BoneParentOf(const Character& character, PartId part);
// True when parent may hold child: it is a folder (or none) and the
// tree stays a tree.
bool CanHold(const Character& character, PartId child, PartId parent);
// True when the parts form a tree, parents before children, ids unique
// and below next_id, and every drawing is sound.
bool IsSound(const Character& character);

}  // namespace snapper

#endif  // SNAPPER_MODEL_CHARACTER_H_
