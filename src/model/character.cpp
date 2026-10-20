#include "model/character.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <set>

namespace snapper {

bool IsFolder(PartKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kPartKindCount);
  return kind == PartKind::kGroup || kind == PartKind::kBone ||
         kind == PartKind::kSwitch;
}

QPointF TipOf(const Bone& bone) {
  assert(std::isfinite(bone.angle));
  assert(bone.length >= 0.0);
  const double radians = bone.angle * std::numbers::pi / 180.0;
  return bone.base + QPointF(std::cos(radians), std::sin(radians)) * bone.length;
}

int PartIndex(const Character& character, PartId id) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(id.value() >= 0);
  const auto found = std::ranges::find_if(
      character.parts,
      [id](const CharacterPart& part) { return part.id == id; });
  return found == character.parts.end()
             ? -1
             : static_cast<int>(found - character.parts.begin());
}

const CharacterPart* FindPart(const Character& character, PartId id) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(id.value() >= 0);
  const int index = PartIndex(character, id);
  return index < 0 ? nullptr : &character.parts[static_cast<size_t>(index)];
}

CharacterPart* FindPart(Character* character, PartId id) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  const CharacterPart* part =
      FindPart(static_cast<const Character&>(*character), id);
  return const_cast<CharacterPart*>(part);
}

const CharacterPart* FindPart(const Character& character,
                              const QString& name) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(name.size() < 100000);
  const auto found = std::ranges::find_if(
      character.parts,
      [&name](const CharacterPart& part) { return part.name == name; });
  return found == character.parts.end() ? nullptr : &*found;
}

const CharacterPart* BoneOwner(const Character& character, BoneId id) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(id.value() >= 0);
  for (const CharacterPart& part : character.parts) {
    const bool has_bone = std::ranges::any_of(
        part.bones, [id](const Bone& bone) { return bone.id == id; });
    if (has_bone) {
      return &part;
    }
  }
  return nullptr;
}

const Bone* FindBone(const Character& character, BoneId id) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(id.value() >= 0);
  const CharacterPart* owner = BoneOwner(character, id);
  if (owner == nullptr) {
    return nullptr;
  }
  const auto found = std::ranges::find_if(
      owner->bones, [id](const Bone& bone) { return bone.id == id; });
  return &*found;
}

Bone* FindBone(Character* character, BoneId id) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  const Bone* bone = FindBone(static_cast<const Character&>(*character), id);
  return const_cast<Bone*>(bone);
}

const SmartAction* FindAction(const Character& character, BoneId bone) {
  assert(character.actions.size() <= static_cast<size_t>(kMaxSmartActions));
  assert(bone.value() >= 0);
  const auto found = std::ranges::find_if(
      character.actions,
      [bone](const SmartAction& action) { return action.bone == bone; });
  return found == character.actions.end() ? nullptr : &*found;
}

SmartAction* FindAction(Character* character, BoneId bone) {
  assert(character != nullptr);
  assert(bone.value() >= 0);
  const SmartAction* action =
      FindAction(static_cast<const Character&>(*character), bone);
  return const_cast<SmartAction*>(action);
}

std::vector<PartId> ChildrenOf(const Character& character, PartId id) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(id.value() >= 0);
  std::vector<PartId> children;
  for (const CharacterPart& part : character.parts) {
    if (part.parent == id) {
      children.push_back(part.id);
    }
  }
  return children;
}

const CharacterPart* BoneParentOf(const Character& character, PartId part) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(part.value() >= 0);
  const CharacterPart* at = FindPart(character, part);
  int steps = 0;
  while (at != nullptr && at->parent.IsValid() && steps < kMaxCharacterParts) {
    at = FindPart(character, at->parent);
    if (at != nullptr && at->kind == PartKind::kBone) {
      return at;
    }
    ++steps;
  }
  return nullptr;
}

bool CanHold(const Character& character, PartId child, PartId parent) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(child.IsValid());
  if (!parent.IsValid()) {
    return true;
  }
  const CharacterPart* at = FindPart(character, parent);
  if (at == nullptr || !IsFolder(at->kind)) {
    return false;
  }
  int steps = 0;
  while (at != nullptr && steps <= kMaxCharacterParts) {
    if (at->id == child) {
      return false;
    }
    at = at->parent.IsValid() ? FindPart(character, at->parent) : nullptr;
    ++steps;
  }
  return true;
}

namespace {

// Adds id to seen; false when it was there already or is out of range.
bool NoteId(int id, int next_id, std::set<int>* seen) {
  assert(seen != nullptr);
  assert(next_id >= 1);
  const bool is_in_range = id > 0 && id < next_id;
  return is_in_range && seen->insert(id).second;
}

bool AreIdsSound(const Character& character) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(character.next_id >= 1);
  std::set<int> seen;
  std::set<int> parts_before;
  int bone_count = 0;
  for (const CharacterPart& part : character.parts) {
    const bool is_parent_before =
        !part.parent.IsValid() || parts_before.contains(part.parent.value());
    if (!is_parent_before ||
        !NoteId(part.id.value(), character.next_id, &seen)) {
      return false;
    }
    parts_before.insert(part.id.value());
    bone_count += static_cast<int>(part.bones.size());
    for (const Bone& bone : part.bones) {
      if (!NoteId(bone.id.value(), character.next_id, &seen)) {
        return false;
      }
    }
    for (const VectorPoint& point : part.drawing.points) {
      if (!NoteId(point.id.value(), character.next_id, &seen)) {
        return false;
      }
    }
  }
  for (const VectorStyle& style : character.styles) {
    if (!NoteId(style.id.value(), character.next_id, &seen)) {
      return false;
    }
  }
  return bone_count <= kMaxCharacterBones;
}

}  // namespace

const VectorStyle* FindStyle(const Character& character, StyleId id) {
  assert(id.value() >= 0);
  assert(character.styles.size() <= static_cast<size_t>(kMaxCharacterStyles));
  const auto found = std::ranges::find_if(
      character.styles, [id](const VectorStyle& style) { return style.id == id; });
  return found == character.styles.end() ? nullptr : &*found;
}

VectorStyle* FindStyle(Character* character, StyleId id) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  const auto found = std::ranges::find_if(
      character->styles,
      [id](const VectorStyle& style) { return style.id == id; });
  return found == character->styles.end() ? nullptr : &*found;
}

VectorShape LookOf(const Character& character, const VectorShape& shape) {
  assert(shape.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  assert(shape.stroke_width >= 0.0);
  const VectorStyle* style =
      shape.style.IsValid() ? FindStyle(character, shape.style) : nullptr;
  if (style == nullptr) {
    return shape;
  }
  VectorShape look = style->look;
  look.curves = shape.curves;
  look.style = shape.style;
  return look;
}

bool IsSound(const Character& character) {
  assert(character.parts.size() < 1000000);
  assert(character.actions.size() < 1000000);
  const bool is_small =
      character.parts.size() <= static_cast<size_t>(kMaxCharacterParts) &&
      character.actions.size() <= static_cast<size_t>(kMaxSmartActions) &&
      character.styles.size() <= static_cast<size_t>(kMaxCharacterStyles);
  if (!is_small || !AreIdsSound(character)) {
    return false;
  }
  const bool are_folders_sound = std::ranges::all_of(
      character.parts, [&character](const CharacterPart& part) {
        const CharacterPart* parent =
            part.parent.IsValid() ? FindPart(character, part.parent) : nullptr;
        return !part.parent.IsValid() ||
               (parent != nullptr && IsFolder(parent->kind));
      });
  const bool are_drawings_sound = std::ranges::all_of(
      character.parts,
      [](const CharacterPart& part) { return IsSound(part.drawing); });
  return are_folders_sound && are_drawings_sound;
}

}  // namespace snapper
