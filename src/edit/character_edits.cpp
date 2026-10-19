#include "edit/character_edits.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <numbers>
#include <set>

#include "base/text.h"

namespace snapper {
namespace {

Error NoPart() { return Error{Tr("That part is gone.")}; }
Error NoBone() { return Error{Tr("That bone is gone.")}; }

int NewId(Character* character) {
  assert(character != nullptr);
  assert(character->next_id >= 1);
  return character->next_id++;
}

// True when id is part or inside it.
bool IsInside(const Character& character, PartId id, PartId part) {
  assert(part.IsValid());
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  const CharacterPart* at = FindPart(character, id);
  int steps = 0;
  while (at != nullptr && steps <= kMaxCharacterParts) {
    if (at->id == part) {
      return true;
    }
    at = at->parent.IsValid() ? FindPart(character, at->parent) : nullptr;
    ++steps;
  }
  return false;
}

// part and everything inside it, in list order.
std::vector<CharacterPart> TakeBlock(Character* character, PartId part) {
  assert(character != nullptr);
  assert(part.IsValid());
  std::vector<CharacterPart> block;
  std::vector<CharacterPart> rest;
  for (const CharacterPart& item : character->parts) {
    (IsInside(*character, item.id, part) ? block : rest).push_back(item);
  }
  character->parts = std::move(rest);
  return block;
}

// Where a part placed in front of parent's children goes in the list:
// just past the last part inside parent (the end for the top).
size_t EndOf(const Character& character, PartId parent) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(parent.value() >= 0);
  if (!parent.IsValid()) {
    return character.parts.size();
  }
  size_t end = static_cast<size_t>(PartIndex(character, parent)) + 1;
  for (size_t i = 0; i < character.parts.size(); ++i) {
    if (IsInside(character, character.parts[i].id, parent)) {
      end = std::max(end, i + 1);
    }
  }
  return end;
}

constexpr double kDegrees = 180.0 / std::numbers::pi;

}  // namespace

QString FreshPartName(const Character& character, const QString& name) {
  assert(!name.isEmpty());
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  QString fresh = name;
  for (int number = 2; FindPart(character, fresh) != nullptr; ++number) {
    fresh = QStringLiteral("%1 %2").arg(name).arg(number);
  }
  return fresh;
}

Character NewCharacter(const QString& name) {
  assert(!name.isEmpty());
  Character character;
  character.name = name;
  const Result<PartId> bones =
      AddPart(&character, PartKind::kBone, Tr("Bones"), PartId());
  assert(bones.has_value());
  const Result<PartId> drawing =
      AddPart(&character, PartKind::kVector, Tr("Drawing"), *bones);
  assert(drawing.has_value() && IsSound(character));
  return character;
}

Result<PartId> AddPart(Character* character, PartKind kind,
                       const QString& name, PartId parent) {
  assert(character != nullptr);
  assert(static_cast<int>(kind) < kPartKindCount);
  if (character->parts.size() >= static_cast<size_t>(kMaxCharacterParts)) {
    return std::unexpected(Error{Tr("The character has too many parts.")});
  }
  const CharacterPart* holder =
      parent.IsValid() ? FindPart(*character, parent) : nullptr;
  if (parent.IsValid() && (holder == nullptr || !IsFolder(holder->kind))) {
    return std::unexpected(
        Error{Tr("Only a group, bone or switch part can hold parts.")});
  }
  CharacterPart part;
  part.id = PartId(NewId(character));
  part.name = FreshPartName(*character, name.isEmpty() ? Tr("Part") : name);
  part.kind = kind;
  part.parent = parent;
  const size_t at = EndOf(*character, parent);
  character->parts.insert(character->parts.begin() + static_cast<long>(at),
                          std::move(part));
  return character->parts[at].id;
}

Result<void> RemoveParts(Character* character, std::span<const PartId> ids) {
  assert(character != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxCharacterParts));
  std::set<BoneId> gone_bones;
  for (PartId id : ids) {
    if (FindPart(*character, id) == nullptr) {
      continue;
    }
    for (const CharacterPart& part : TakeBlock(character, id)) {
      for (const Bone& bone : part.bones) {
        gone_bones.insert(bone.id);
      }
    }
  }
  std::erase_if(character->actions, [&gone_bones](const SmartAction& action) {
    return gone_bones.contains(action.bone);
  });
  for (CharacterPart& part : character->parts) {
    std::erase_if(part.point_bones, [&gone_bones](const auto& entry) {
      return gone_bones.contains(entry.second);
    });
    if (gone_bones.contains(part.bound_bone)) {
      part.bound_bone = BoneId();
    }
    if (part.rest_pick.IsValid() && FindPart(*character, part.rest_pick) == nullptr) {
      part.rest_pick = PartId();
    }
  }
  return {};
}

Result<void> RenamePart(Character* character, PartId id, const QString& name) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  CharacterPart* part = FindPart(character, id);
  if (part == nullptr) {
    return std::unexpected(NoPart());
  }
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    return std::unexpected(Error{Tr("A part needs a name.")});
  }
  const CharacterPart* same = FindPart(*character, trimmed);
  if (same != nullptr && same->id != id) {
    return std::unexpected(Error{Tr("Another part is called %1.").arg(trimmed)});
  }
  part->name = trimmed;
  return {};
}

Result<void> MovePart(Character* character, PartId id, PartId parent,
                      int place) {
  assert(character != nullptr);
  assert(place >= -1000000);
  if (FindPart(*character, id) == nullptr) {
    return std::unexpected(NoPart());
  }
  if (!CanHold(*character, id, parent)) {
    return std::unexpected(Error{Tr("That part can't go there.")});
  }
  std::vector<CharacterPart> block = TakeBlock(character, id);
  block.front().parent = parent;
  const std::vector<PartId> siblings = ChildrenOf(*character, parent);
  const int count = static_cast<int>(siblings.size());
  const int to = std::clamp(place, 0, count);
  const size_t at =
      to == count
          ? EndOf(*character, parent)
          : static_cast<size_t>(
                PartIndex(*character, siblings[static_cast<size_t>(to)]));
  character->parts.insert(character->parts.begin() + static_cast<long>(at),
                          block.begin(), block.end());
  return {};
}

Result<PartId> DuplicatePart(Character* character, PartId id) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  if (FindPart(*character, id) == nullptr) {
    return std::unexpected(NoPart());
  }
  std::vector<CharacterPart> block;
  for (const CharacterPart& part : character->parts) {
    if (IsInside(*character, part.id, id)) {
      block.push_back(part);
    }
  }
  if (character->parts.size() + block.size() >
      static_cast<size_t>(kMaxCharacterParts)) {
    return std::unexpected(Error{Tr("The character has too many parts.")});
  }
  std::map<int, int> ids;
  for (const CharacterPart& part : block) {
    ids[part.id.value()] = NewId(character);
    for (const Bone& bone : part.bones) ids[bone.id.value()] = NewId(character);
    for (const VectorPoint& point : part.drawing.points) {
      ids[point.id.value()] = NewId(character);
    }
  }
  const auto map = [&ids](int value) {
    const auto found = ids.find(value);
    return found == ids.end() ? value : found->second;
  };
  for (CharacterPart& part : block) {
    part.id = PartId(map(part.id.value()));
    part.parent = PartId(map(part.parent.value()));
    part.rest_pick = PartId(map(part.rest_pick.value()));
    part.bound_bone = BoneId(map(part.bound_bone.value()));
    for (Bone& bone : part.bones) {
      bone.id = BoneId(map(bone.id.value()));
      bone.parent = BoneId(map(bone.parent.value()));
    }
    for (VectorPoint& point : part.drawing.points) {
      point.id = PointId(map(point.id.value()));
    }
    for (VectorCurve& curve : part.drawing.curves) {
      for (PointId& point : curve.points) point = PointId(map(point.value()));
    }
    std::map<PointId, BoneId> bound;
    for (const auto& [point, bone] : part.point_bones) {
      bound[PointId(map(point.value()))] = BoneId(map(bone.value()));
    }
    part.point_bones = std::move(bound);
  }
  block.front().name = FreshPartName(*character, block.front().name);
  for (size_t i = 1; i < block.size(); ++i) {
    block[i].name = FreshPartName(*character, block[i].name);
  }
  const size_t at = EndOf(*character, id);
  character->parts.insert(character->parts.begin() + static_cast<long>(at),
                          block.begin(), block.end());
  return character->parts[at].id;
}

Result<void> SetVisible(Character* character, PartId id, bool is_visible) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  CharacterPart* part = FindPart(character, id);
  if (part == nullptr) {
    return std::unexpected(NoPart());
  }
  part->is_visible = is_visible;
  return {};
}

Result<void> SetPartRest(Character* character, PartId id,
                         const CharacterPart& rest) {
  assert(character != nullptr);
  assert(std::isfinite(rest.rotation));
  CharacterPart* part = FindPart(character, id);
  if (part == nullptr) {
    return std::unexpected(NoPart());
  }
  part->origin = rest.origin;
  part->offset = rest.offset;
  part->rotation = rest.rotation;
  part->scale_x = rest.scale_x;
  part->scale_y = rest.scale_y;
  part->is_flipped = rest.is_flipped;
  part->opacity = std::clamp(rest.opacity, 0.0, 1.0);
  return {};
}

Result<void> SetBind(Character* character, PartId id, BindMode bind,
                     BoneId bone) {
  assert(character != nullptr);
  assert(static_cast<int>(bind) < kBindModeCount);
  CharacterPart* part = FindPart(character, id);
  if (part == nullptr) {
    return std::unexpected(NoPart());
  }
  const CharacterPart* bones = BoneParentOf(*character, id);
  const bool is_bone_there =
      bones != nullptr &&
      std::ranges::any_of(bones->bones,
                          [bone](const Bone& b) { return b.id == bone; });
  if (bind == BindMode::kLayer && !is_bone_there) {
    return std::unexpected(
        Error{Tr("Pick a bone of the bone part around this part.")});
  }
  part->bind = bind;
  part->bound_bone = bind == BindMode::kLayer ? bone : BoneId();
  return {};
}

Result<void> SetRestPick(Character* character, PartId id, PartId pick) {
  assert(character != nullptr);
  assert(pick.value() >= 0);
  CharacterPart* part = FindPart(character, id);
  if (part == nullptr || part->kind != PartKind::kSwitch) {
    return std::unexpected(NoPart());
  }
  const CharacterPart* child = FindPart(*character, pick);
  if (child == nullptr || child->parent != id) {
    return std::unexpected(Error{Tr("That part isn't in this switch.")});
  }
  part->rest_pick = pick;
  return {};
}

Result<void> BindPoints(Character* character, PartId id,
                        std::span<const PointId> points, BoneId bone) {
  assert(character != nullptr);
  assert(points.size() <= static_cast<size_t>(kMaxVectorPoints));
  CharacterPart* part = FindPart(character, id);
  if (part == nullptr || part->kind != PartKind::kVector) {
    return std::unexpected(NoPart());
  }
  const CharacterPart* bones = BoneParentOf(*character, id);
  const bool is_bone_there =
      !bone.IsValid() ||
      (bones != nullptr &&
       std::ranges::any_of(bones->bones,
                           [bone](const Bone& b) { return b.id == bone; }));
  if (!is_bone_there) {
    return std::unexpected(
        Error{Tr("Pick a bone of the bone part around this part.")});
  }
  for (PointId point : points) {
    if (bone.IsValid()) {
      part->point_bones[point] = bone;
    } else {
      part->point_bones.erase(point);
    }
  }
  part->bind = BindMode::kPoints;
  return {};
}

Result<BoneId> AddBone(Character* character, PartId bone_part, QPointF base,
                       QPointF tip, BoneId parent) {
  assert(character != nullptr);
  assert(std::isfinite(base.x()) && std::isfinite(tip.x()));
  CharacterPart* part = FindPart(character, bone_part);
  if (part == nullptr || part->kind != PartKind::kBone) {
    return std::unexpected(Error{Tr("Bones go in a bone part.")});
  }
  int count = 0;
  for (const CharacterPart& item : character->parts) {
    count += static_cast<int>(item.bones.size());
  }
  if (count >= kMaxCharacterBones) {
    return std::unexpected(Error{Tr("The character has too many bones.")});
  }
  const bool is_parent_here =
      !parent.IsValid() ||
      std::ranges::any_of(part->bones,
                          [parent](const Bone& b) { return b.id == parent; });
  if (!is_parent_here) {
    return std::unexpected(NoBone());
  }
  Bone bone;
  bone.id = BoneId(NewId(character));
  bone.name = Tr("Bone %1").arg(part->bones.size() + 1);
  bone.parent = parent;
  bone.base = base;
  const QLineF line(base, tip);
  bone.length = line.length();
  bone.angle = std::atan2(tip.y() - base.y(), tip.x() - base.x()) * kDegrees;
  bone.strength = std::max(20.0, bone.length * 0.6);
  part->bones.push_back(bone);
  return bone.id;
}

Result<void> SetBone(Character* character, const Bone& changed) {
  assert(character != nullptr);
  assert(changed.length >= 0.0);
  Bone* bone = FindBone(character, changed.id);
  if (bone == nullptr) {
    return std::unexpected(NoBone());
  }
  const CharacterPart* owner = BoneOwner(*character, changed.id);
  BoneId up = changed.parent;
  int steps = 0;
  while (up.IsValid() && steps <= kMaxCharacterBones) {
    const auto found = std::ranges::find_if(
        owner->bones, [up](const Bone& b) { return b.id == up; });
    if (found == owner->bones.end() || up == changed.id) {
      return std::unexpected(Error{Tr("A bone can't hang from that bone.")});
    }
    up = found->parent;
    ++steps;
  }
  *bone = changed;
  if (bone->min_turn > bone->max_turn) {
    std::swap(bone->min_turn, bone->max_turn);
  }
  bone->strength = std::max(0.0, bone->strength);
  bone->dynamics = std::clamp(bone->dynamics, 0.0, 1.0);
  if (bone->name.trimmed().isEmpty()) {
    bone->name = Tr("Bone");
  }
  return {};
}

Result<void> SetBoneRest(Character* character, BoneId id, QPointF base,
                         QPointF tip) {
  assert(character != nullptr);
  assert(std::isfinite(base.x()) && std::isfinite(tip.x()));
  Bone* bone = FindBone(character, id);
  if (bone == nullptr) {
    return std::unexpected(NoBone());
  }
  bone->base = base;
  bone->length = QLineF(base, tip).length();
  bone->angle = std::atan2(tip.y() - base.y(), tip.x() - base.x()) * kDegrees;
  return {};
}

Result<void> RemoveBones(Character* character, std::span<const BoneId> ids) {
  assert(character != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxCharacterBones));
  const std::set<BoneId> gone(ids.begin(), ids.end());
  for (CharacterPart& part : character->parts) {
    for (Bone& bone : part.bones) {
      while (gone.contains(bone.parent)) {
        const auto up = std::ranges::find_if(
            part.bones, [&bone](const Bone& b) { return b.id == bone.parent; });
        bone.parent = up == part.bones.end() ? BoneId() : up->parent;
      }
    }
    std::erase_if(part.bones,
                  [&gone](const Bone& bone) { return gone.contains(bone.id); });
    std::erase_if(part.point_bones, [&gone](const auto& entry) {
      return gone.contains(entry.second);
    });
    if (gone.contains(part.bound_bone)) {
      part.bound_bone = BoneId();
      part.bind = BindMode::kFlexi;
    }
  }
  std::erase_if(character->actions, [&gone](const SmartAction& action) {
    return gone.contains(action.bone);
  });
  return {};
}

Result<void> SetSmart(Character* character, BoneId bone, bool is_smart) {
  assert(character != nullptr);
  assert(bone.value() >= 0);
  Bone* found = FindBone(character, bone);
  if (found == nullptr) {
    return std::unexpected(NoBone());
  }
  found->is_smart = is_smart;
  const bool has_action = FindAction(*character, bone) != nullptr;
  if (is_smart && !has_action) {
    if (character->actions.size() >= static_cast<size_t>(kMaxSmartActions)) {
      return std::unexpected(Error{Tr("The character has too many actions.")});
    }
    character->actions.push_back(SmartAction{bone, found->name, {}});
  }
  if (!is_smart) {
    std::erase_if(character->actions, [bone](const SmartAction& action) {
      return action.bone == bone;
    });
  }
  return {};
}

Result<int> AddActionStop(Character* character, BoneId bone, double turn) {
  assert(character != nullptr);
  assert(std::isfinite(turn));
  SmartAction* action = FindAction(character, bone);
  if (action == nullptr) {
    return std::unexpected(Error{Tr("Make the bone a smart bone first.")});
  }
  if (std::abs(turn) < 0.5) {
    return std::unexpected(
        Error{Tr("Turn the smart bone away from rest to set a stop.")});
  }
  const auto same = std::ranges::find_if(action->stops, [turn](const ActionStop& s) {
    return std::abs(s.turn - turn) < 0.5;
  });
  if (same != action->stops.end()) {
    return static_cast<int>(same - action->stops.begin());
  }
  if (action->stops.size() >= static_cast<size_t>(kMaxActionStops)) {
    return std::unexpected(Error{Tr("The action has too many stops.")});
  }
  action->stops.push_back(ActionStop{turn, {}});
  std::ranges::sort(action->stops, {}, &ActionStop::turn);
  const auto added = std::ranges::find_if(
      action->stops, [turn](const ActionStop& s) { return s.turn == turn; });
  return static_cast<int>(added - action->stops.begin());
}

Result<void> SetActionStop(Character* character, BoneId bone, int stop,
                           const CharacterPose& pose) {
  assert(character != nullptr);
  assert(stop >= -1);
  SmartAction* action = FindAction(character, bone);
  if (action == nullptr || stop < 0 ||
      static_cast<size_t>(stop) >= action->stops.size()) {
    return std::unexpected(Error{Tr("That stop is gone.")});
  }
  CharacterPose cleaned = pose;
  cleaned.bones.erase(bone);
  action->stops[static_cast<size_t>(stop)].pose = std::move(cleaned);
  return {};
}

Result<void> RemoveActionStop(Character* character, BoneId bone, int stop) {
  assert(character != nullptr);
  assert(stop >= -1);
  SmartAction* action = FindAction(character, bone);
  if (action == nullptr || stop < 0 ||
      static_cast<size_t>(stop) >= action->stops.size()) {
    return std::unexpected(Error{Tr("That stop is gone.")});
  }
  action->stops.erase(action->stops.begin() + stop);
  return {};
}

}  // namespace snapper
