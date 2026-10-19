#include "anim/character_pose.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <set>

#include "anim/sampler.h"

namespace snapper {
namespace {

// Where a pick swaps between two poses: at the next key, or halfway.
enum class PickSwap { kAtEnd, kHalfway };

const PartPose& PartOr(const std::map<PartId, PartPose>& parts, PartId id) {
  static const PartPose kRest;
  assert(parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(id.IsValid());
  const auto found = parts.find(id);
  return found == parts.end() ? kRest : found->second;
}

const BonePose& BoneOr(const std::map<BoneId, BonePose>& bones, BoneId id) {
  static const BonePose kRest;
  assert(bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  assert(id.IsValid());
  const auto found = bones.find(id);
  return found == bones.end() ? kRest : found->second;
}

PartPose BlendPart(const PartPose& a, const PartPose& b, double t,
                   PickSwap swap) {
  assert(t >= -1e-9);
  assert(t <= 1.0 + 1e-9);
  PartPose out;
  out.offset = Lerp(a.offset, b.offset, t);
  out.rotation = Lerp(a.rotation, b.rotation, t);
  out.scale_x = Lerp(a.scale_x, b.scale_x, t);
  out.scale_y = Lerp(a.scale_y, b.scale_y, t);
  out.opacity = Lerp(a.opacity, b.opacity, t);
  const bool is_b = swap == PickSwap::kHalfway ? t >= 0.5 : t >= 1.0;
  out.order = is_b ? b.order : a.order;
  out.pick = is_b ? b.pick : a.pick;
  std::set<PointId> ids;
  for (const auto& [id, at] : a.points) ids.insert(id);
  for (const auto& [id, at] : b.points) ids.insert(id);
  for (PointId id : ids) {
    const auto in_a = a.points.find(id);
    const auto in_b = b.points.find(id);
    const QPointF from = in_a == a.points.end() ? QPointF() : in_a->second;
    const QPointF to = in_b == b.points.end() ? QPointF() : in_b->second;
    out.points[id] = Lerp(from, to, t);
  }
  return out;
}

CharacterPose BlendPose(const CharacterPose& a, const CharacterPose& b,
                        double t, PickSwap swap) {
  assert(a.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(b.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  CharacterPose out;
  out.root = BlendPart(a.root, b.root, t, swap);
  std::set<PartId> parts;
  for (const auto& [id, part] : a.parts) parts.insert(id);
  for (const auto& [id, part] : b.parts) parts.insert(id);
  for (PartId id : parts) {
    out.parts[id] = BlendPart(PartOr(a.parts, id), PartOr(b.parts, id), t, swap);
  }
  std::set<BoneId> bones;
  for (const auto& [id, bone] : a.bones) bones.insert(id);
  for (const auto& [id, bone] : b.bones) bones.insert(id);
  for (BoneId id : bones) {
    const BonePose& from = BoneOr(a.bones, id);
    const BonePose& to = BoneOr(b.bones, id);
    out.bones[id] = {Lerp(from.turn, to.turn, t),
                     Lerp(from.offset, to.offset, t),
                     Lerp(from.scale, to.scale, t)};
  }
  return out;
}

PartPose AddPart(const PartPose& pose, const PartPose& delta) {
  assert(pose.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(delta.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  PartPose out = pose;
  out.offset += delta.offset;
  out.rotation += delta.rotation;
  out.scale_x *= delta.scale_x;
  out.scale_y *= delta.scale_y;
  out.opacity *= delta.opacity;
  out.order += delta.order;
  if (delta.pick.IsValid()) {
    out.pick = delta.pick;
  }
  for (const auto& [id, at] : delta.points) {
    out.points[id] += at;
  }
  return out;
}

}  // namespace

CharacterPose Lerp(const CharacterPose& a, const CharacterPose& b, double t) {
  assert(t >= -1e-9);
  assert(t <= 1.0 + 1e-9);
  return BlendPose(a, b, t, PickSwap::kAtEnd);
}

CharacterPose AddPose(const CharacterPose& pose, const CharacterPose& delta) {
  assert(pose.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(delta.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  CharacterPose out = pose;
  out.root = AddPart(pose.root, delta.root);
  for (const auto& [id, part] : delta.parts) {
    out.parts[id] = AddPart(PartOr(pose.parts, id), part);
  }
  for (const auto& [id, bone] : delta.bones) {
    BonePose& to = out.bones[id];
    to.turn += bone.turn;
    to.offset += bone.offset;
    to.scale *= bone.scale;
  }
  return out;
}

CharacterPose ActionAt(const SmartAction& action, double turn) {
  assert(action.stops.size() <= static_cast<size_t>(kMaxActionStops));
  assert(std::isfinite(turn));
  std::vector<ActionStop> stops = action.stops;
  const bool has_rest = std::ranges::any_of(
      stops, [](const ActionStop& stop) { return stop.turn == 0.0; });
  if (!has_rest) {
    stops.push_back(ActionStop{});
  }
  std::ranges::sort(stops, {}, &ActionStop::turn);
  if (turn <= stops.front().turn) {
    return stops.front().pose;
  }
  if (turn >= stops.back().turn) {
    return stops.back().pose;
  }
  const auto next = std::ranges::find_if(
      stops, [turn](const ActionStop& stop) { return stop.turn > turn; });
  const ActionStop& from = *(next - 1);
  const double t = (turn - from.turn) / (next->turn - from.turn);
  return BlendPose(from.pose, next->pose, t, PickSwap::kHalfway);
}

CharacterPose WithActions(const Character& character,
                          const CharacterPose& hand) {
  assert(character.actions.size() <= static_cast<size_t>(kMaxSmartActions));
  assert(hand.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  CharacterPose out = hand;
  for (const SmartAction& action : character.actions) {
    const Bone* bone = FindBone(character, action.bone);
    if (bone == nullptr) {
      continue;
    }
    CharacterPose delta = ActionAt(action, BoneOr(hand.bones, action.bone).turn);
    delta.bones.erase(action.bone);
    out = AddPose(out, delta);
  }
  return out;
}

CharacterPose PoseAt(const Character& character, const CharacterLayer& layer,
                     Frame frame) {
  assert(frame.index() >= 0);
  assert(IsSorted(layer.poses));
  return WithActions(character, Sample(layer.poses, frame, CharacterPose{}));
}

namespace {

constexpr double kDegrees = 180.0 / std::numbers::pi;

QPointF Turned(QPointF point, double degrees) {
  assert(std::isfinite(degrees));
  assert(std::isfinite(point.x()));
  const double radians = degrees / kDegrees;
  const double c = std::cos(radians);
  const double s = std::sin(radians);
  return {point.x() * c - point.y() * s, point.x() * s + point.y() * c};
}

// A bone's frame: where it starts, which way it points, its stretch.
struct BoneFrame final {
  QPointF origin;
  double angle = 0.0;
  double scale = 1.0;
};

// Moves a point from where bone was at rest to where frame puts it.
QPointF Bend(const Bone& bone, const BoneFrame& frame, QPointF at) {
  assert(std::isfinite(frame.angle));
  assert(std::isfinite(at.x()));
  QPointF local = Turned(at - bone.base, -bone.angle);
  local.setX(local.x() * frame.scale);
  return frame.origin + Turned(local, frame.angle);
}

QTransform BendTransform(const Bone& bone, const BoneFrame& frame) {
  assert(std::isfinite(frame.angle));
  assert(std::isfinite(frame.scale));
  QTransform bend;
  bend.translate(frame.origin.x(), frame.origin.y());
  bend.rotate(frame.angle);
  bend.scale(frame.scale, 1.0);
  bend.rotate(-bone.angle);
  bend.translate(-bone.base.x(), -bone.base.y());
  return bend;
}

// Every bone frame of a bone part as posed, in the part's bone order.
std::vector<BoneFrame> PoseFrames(const CharacterPart& part,
                                 const CharacterPose& pose) {
  assert(part.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  assert(pose.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  const size_t count = part.bones.size();
  std::vector<BoneFrame> frames(count);
  std::vector<bool> is_done(count, false);
  for (size_t round = 0; round <= count; ++round) {
    for (size_t i = 0; i < count; ++i) {
      const Bone& bone = part.bones[i];
      const auto parent = std::ranges::find_if(
          part.bones, [&bone](const Bone& b) { return b.id == bone.parent; });
      const size_t p = static_cast<size_t>(parent - part.bones.begin());
      const bool is_root = parent == part.bones.end();
      if (is_done[i] || (!is_root && !is_done[p])) {
        continue;
      }
      const BonePose& own = BoneOr(pose.bones, bone.id);
      BoneFrame frame{bone.base + own.offset, bone.angle + own.turn, own.scale};
      if (!is_root) {
        const BoneFrame& up = frames[p];
        QPointF local = Turned(bone.base - parent->base, -parent->angle);
        local.setX(local.x() * up.scale);
        frame.origin = up.origin + Turned(local, up.angle) + own.offset;
        frame.angle = up.angle + (bone.angle - parent->angle) + own.turn;
      }
      frames[i] = frame;
      is_done[i] = true;
    }
  }
  return frames;
}

QTransform LocalOf(const CharacterPart& part, const PartPose& pose) {
  assert(std::isfinite(part.rotation));
  assert(std::isfinite(pose.rotation));
  const QPointF at = part.offset + pose.offset + part.origin;
  const double flip = part.is_flipped ? -1.0 : 1.0;
  QTransform local;
  local.translate(at.x(), at.y());
  local.rotate(part.rotation + pose.rotation);
  local.scale(part.scale_x * pose.scale_x * flip, part.scale_y * pose.scale_y);
  local.translate(-part.origin.x(), -part.origin.y());
  return local;
}

QTransform RootOf(const PartPose& root) {
  assert(std::isfinite(root.rotation));
  assert(std::isfinite(root.scale_x));
  QTransform transform;
  transform.translate(root.offset.x(), root.offset.y());
  transform.rotate(root.rotation);
  transform.scale(root.scale_x, root.scale_y);
  return transform;
}

// Where each part sits, for one pose.
struct Placed final {
  // To the nearest bone part's space (or the character's, with none),
  // and whether a part on the way rides one bone (then flexi is off).
  QTransform to_bones;
  bool is_carried = false;
  QTransform bones_to_character;
  QTransform to_character;
  // kBone: its bones as posed.
  std::vector<BoneFrame> frames;
};

class Placer final {
 public:
  Placer(const Character& character, const CharacterPose& pose)
      : character_(character), pose_(pose), placed_(character.parts.size()) {
    assert(IsSound(character));
    assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
    for (size_t i = 0; i < character.parts.size(); ++i) {
      Place(i);
    }
  }

  const Placed& at(size_t index) const { return placed_[index]; }

 private:
  void Place(size_t index) {
    assert(index < character_.parts.size());
    assert(placed_.size() == character_.parts.size());
    const CharacterPart& part = character_.parts[index];
    const PartPose& own = PartOr(pose_.parts, part.id);
    Placed& placed = placed_[index];
    const int parent =
        part.parent.IsValid() ? PartIndex(character_, part.parent) : -1;
    const QTransform local = LocalOf(part, own);
    if (parent < 0) {
      placed.to_bones = local;
      placed.bones_to_character = RootOf(pose_.root);
    } else {
      const CharacterPart& up = character_.parts[static_cast<size_t>(parent)];
      const Placed& above = placed_[static_cast<size_t>(parent)];
      const bool is_bone_parent = up.kind == PartKind::kBone;
      placed.to_bones = is_bone_parent ? local : local * above.to_bones;
      placed.is_carried = !is_bone_parent && above.is_carried;
      placed.bones_to_character =
          is_bone_parent ? above.to_character : above.bones_to_character;
      Ride(part, &placed);
    }
    placed.to_character = placed.to_bones * placed.bones_to_character;
    if (part.kind == PartKind::kBone) {
      placed.frames = PoseFrames(part, pose_);
    }
  }

  // A part riding one bone takes that bone's bend.
  void Ride(const CharacterPart& part, Placed* placed) const {
    assert(placed != nullptr);
    assert(part.id.IsValid());
    if (part.bind != BindMode::kLayer || !part.bound_bone.IsValid()) {
      return;
    }
    const CharacterPart* owner = BoneParentOf(character_, part.id);
    if (owner == nullptr) {
      return;
    }
    const auto bone = std::ranges::find_if(
        owner->bones, [&part](const Bone& b) { return b.id == part.bound_bone; });
    if (bone == owner->bones.end()) {
      return;
    }
    const Placed& owner_placed =
        placed_[static_cast<size_t>(PartIndex(character_, owner->id))];
    const size_t b = static_cast<size_t>(bone - owner->bones.begin());
    placed->to_bones =
        placed->to_bones * BendTransform(*bone, owner_placed.frames[b]);
    placed->is_carried = true;
  }

  const Character& character_;
  const CharacterPose& pose_;
  std::vector<Placed> placed_;
};

// The shown children of a folder, back to front, by list place plus the
// pose's order shift.
std::vector<size_t> ShownChildren(const Character& character,
                                  const CharacterPose& pose, PartId parent) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(parent.value() >= 0);
  const CharacterPart* folder =
      parent.IsValid() ? FindPart(character, parent) : nullptr;
  std::vector<std::pair<int, size_t>> children;
  for (size_t i = 0; i < character.parts.size(); ++i) {
    const CharacterPart& part = character.parts[i];
    if (part.parent == parent && part.is_visible) {
      const int shift = PartOr(pose.parts, part.id).order;
      children.emplace_back(static_cast<int>(i) + shift, i);
    }
  }
  std::ranges::stable_sort(children, {}, &std::pair<int, size_t>::first);
  std::vector<size_t> shown;
  for (const auto& [place, index] : children) {
    shown.push_back(index);
  }
  if (folder != nullptr && folder->kind == PartKind::kSwitch && !shown.empty()) {
    const PartId posed = PartOr(pose.parts, folder->id).pick;
    const PartId pick = posed.IsValid() ? posed : folder->rest_pick;
    const auto picked = std::ranges::find_if(shown, [&](size_t i) {
      return character.parts[i].id == pick;
    });
    const size_t only = picked == shown.end() ? shown.front() : *picked;
    shown.assign(1, only);
  }
  return shown;
}

// Bends one rest point (at in the bone part's space) by the bones.
QPointF BendPoint(const CharacterPart& part, const CharacterPart& bones,
                  const std::vector<BoneFrame>& frames, PointId id, QPointF at,
                  QPointF rest_at) {
  assert(frames.size() == bones.bones.size());
  assert(std::isfinite(at.x()));
  if (part.bind == BindMode::kPoints) {
    const auto bound = part.point_bones.find(id);
    if (bound == part.point_bones.end()) {
      return at;
    }
    const auto bone = std::ranges::find_if(
        bones.bones, [&](const Bone& b) { return b.id == bound->second; });
    return bone == bones.bones.end()
               ? at
               : Bend(*bone, frames[static_cast<size_t>(bone - bones.bones.begin())], at);
  }
  const std::vector<double> weights = FlexiWeights(bones, rest_at);
  QPointF sum;
  double total = 0.0;
  for (size_t b = 0; b < weights.size(); ++b) {
    if (weights[b] > 0.0) {
      sum += weights[b] * Bend(bones.bones[b], frames[b], at);
      total += weights[b];
    }
  }
  return total > 0.0 ? sum / total : at;
}

}  // namespace

std::vector<double> FlexiWeights(const CharacterPart& bone_part, QPointF at) {
  assert(bone_part.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  assert(std::isfinite(at.x()));
  const size_t count = bone_part.bones.size();
  std::vector<double> weights(count, 0.0);
  double total = 0.0;
  size_t nearest = count;
  double nearest_gap = 1e300;
  for (size_t b = 0; b < count; ++b) {
    const Bone& bone = bone_part.bones[b];
    if (bone.is_smart) {
      continue;
    }
    const QPointF tip = TipOf(bone);
    const QPointF along = tip - bone.base;
    const double length2 = QPointF::dotProduct(along, along);
    const double t = length2 > 0.0
                         ? std::clamp(QPointF::dotProduct(at - bone.base, along) /
                                          length2, 0.0, 1.0)
                         : 0.0;
    const double gap = QLineF(at, bone.base + t * along).length();
    if (gap < nearest_gap) {
      nearest_gap = gap;
      nearest = b;
    }
    if (bone.strength > 0.0 && gap < bone.strength) {
      const double reach = 1.0 - (gap / bone.strength) * (gap / bone.strength);
      weights[b] = reach * reach;
      total += weights[b];
    }
  }
  if (total <= 0.0 && nearest < count) {
    weights[nearest] = 1.0;
    total = 1.0;
  }
  for (double& weight : weights) {
    weight = total > 0.0 ? weight / total : 0.0;
  }
  return weights;
}

namespace {

std::vector<PosedBone> BonesOf(const Character& character,
                               const Placer& placer) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(IsSound(character));
  std::vector<PosedBone> bones;
  for (size_t i = 0; i < character.parts.size(); ++i) {
    const CharacterPart& part = character.parts[i];
    const Placed& placed = placer.at(i);
    for (size_t b = 0; b < part.bones.size(); ++b) {
      const BoneFrame& frame = placed.frames[b];
      const QPointF tip =
          frame.origin +
          Turned({part.bones[b].length * frame.scale, 0.0}, frame.angle);
      const QPointF base = placed.to_character.map(frame.origin);
      const QPointF end = placed.to_character.map(tip);
      const double angle =
          std::atan2(end.y() - base.y(), end.x() - base.x()) * kDegrees;
      bones.push_back(
          {part.bones[b].id, base, end, angle, part.bones[b].is_smart});
    }
  }
  return bones;
}

class Walker final {
 public:
  Walker(const Character& character, const CharacterPose& pose,
         PosedCharacter* out)
      : character_(character), pose_(pose), posed_(character, pose),
        rest_(character, CharacterPose{}), out_(out) {
    assert(out != nullptr);
    assert(IsSound(character));
    Walk(PartId(), 1.0, 0);
    out->bones = BonesOf(character, posed_);
    for (size_t i = 0; i < character.parts.size(); ++i) {
      out->spaces[character.parts[i].id] = posed_.at(i).to_character;
    }
  }

 private:
  void Walk(PartId parent, double opacity, int depth) {
    assert(depth <= kMaxCharacterParts);
    assert(opacity >= 0.0);
    for (size_t index : ShownChildren(character_, pose_, parent)) {
      const CharacterPart& part = character_.parts[index];
      const double own =
          opacity * part.opacity * PartOr(pose_.parts, part.id).opacity;
      if (IsFolder(part.kind)) {
        Walk(part.id, own, depth + 1);
        continue;
      }
      PosedPart posed{part.id, part.kind, posed_.at(index).to_character, own, {}};
      if (part.kind == PartKind::kVector) {
        posed.points = PointsOf(index);
      }
      out_->parts.push_back(std::move(posed));
    }
  }

  std::vector<QPointF> PointsOf(size_t index) const {
    assert(index < character_.parts.size());
    assert(character_.parts[index].kind == PartKind::kVector);
    const CharacterPart& part = character_.parts[index];
    const Placed& placed = posed_.at(index);
    const Placed& rest = rest_.at(index);
    const CharacterPart* bones = BoneParentOf(character_, part.id);
    const bool is_bent = bones != nullptr && !placed.is_carried &&
                         (part.bind == BindMode::kFlexi ||
                          part.bind == BindMode::kPoints);
    const QTransform& bones_to_character = placed.bones_to_character;
    const Placed* bone_placed =
        bones == nullptr
            ? nullptr
            : &posed_.at(static_cast<size_t>(PartIndex(character_, bones->id)));
    const PartPose& own = PartOr(pose_.parts, part.id);
    std::vector<QPointF> points;
    points.reserve(part.drawing.points.size());
    for (const VectorPoint& point : part.drawing.points) {
      const auto moved = own.points.find(point.id);
      const QPointF at =
          point.at + (moved == own.points.end() ? QPointF() : moved->second);
      QPointF in_bones = placed.to_bones.map(at);
      if (is_bent) {
        in_bones = BendPoint(part, *bones, bone_placed->frames, point.id,
                             in_bones, rest.to_bones.map(point.at));
      }
      points.push_back(bones_to_character.map(in_bones));
    }
    return points;
  }

  const Character& character_;
  const CharacterPose& pose_;
  Placer posed_;
  Placer rest_;
  PosedCharacter* out_;
};

}  // namespace

std::vector<PosedBone> PoseBones(const Character& character,
                                 const CharacterPose& pose) {
  assert(IsSound(character));
  assert(pose.bones.size() <= static_cast<size_t>(kMaxCharacterBones));
  return BonesOf(character, Placer(character, pose));
}

PosedCharacter PoseCharacter(const Character& character,
                             const CharacterPose& pose) {
  assert(IsSound(character));
  assert(pose.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  PosedCharacter out;
  const Walker walker(character, pose, &out);
  return out;
}

}  // namespace snapper
