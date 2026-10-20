#include "ui/character_session.h"

#include <cassert>
#include <algorithm>
#include <cmath>

namespace snapper {

void CharacterSession::SetCharacter(const QString& character) {
  assert(character.size() < 100000);
  assert(character_.size() < 100000);
  if (character == character_) {
    return;
  }
  character_ = character;
  part_ = PartId();
  points_.clear();
  bone_ = BoneId();
  shape_ = -1;
  action_bone_ = BoneId();
  action_stop_ = -1;
  emit Changed();
}

void CharacterSession::SetPart(PartId part) {
  assert(part.value() >= 0);
  assert(points_.size() <= static_cast<size_t>(kMaxVectorPoints));
  if (part == part_) {
    return;
  }
  part_ = part;
  points_.clear();
  shape_ = -1;
  emit Changed();
}

void CharacterSession::SetPoints(std::set<PointId> points) {
  assert(points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(points_.size() <= static_cast<size_t>(kMaxVectorPoints));
  if (points == points_) {
    return;
  }
  points_ = std::move(points);
  emit Changed();
}

void CharacterSession::SetBone(BoneId bone) {
  assert(bone.value() >= 0);
  assert(bone_.value() >= 0);
  if (bone == bone_) {
    return;
  }
  bone_ = bone;
  emit Changed();
}

void CharacterSession::SetShape(int shape) {
  assert(shape >= -1);
  assert(shape_ >= -1);
  if (shape == shape_) {
    return;
  }
  shape_ = shape;
  emit Changed();
}

void CharacterSession::SetTool(CharacterTool tool) {
  assert(static_cast<int>(tool) < kCharacterToolCount);
  assert(static_cast<int>(tool_) < kCharacterToolCount);
  if (tool == tool_) {
    return;
  }
  tool_ = tool;
  emit Changed();
}

void CharacterSession::SetMagnetRadius(double radius) {
  assert(std::isfinite(radius));
  assert(magnet_radius_ > 0.0);
  const double clamped =
      std::clamp(radius, kMinMagnetRadius, kMaxMagnetRadius);
  if (clamped == magnet_radius_) {
    return;
  }
  magnet_radius_ = clamped;
  emit Changed();
}

void CharacterSession::SetAction(BoneId bone, int stop, double turn) {
  assert(stop >= -1);
  assert(std::isfinite(turn));
  const bool is_same =
      bone == action_bone_ && stop == action_stop_ && turn == action_turn_;
  if (is_same) {
    return;
  }
  action_bone_ = bone;
  action_stop_ = stop;
  action_turn_ = turn;
  emit Changed();
}

void CharacterSession::SetStyle(const VectorShape& style) {
  assert(style.stroke_width >= 0.0);
  assert(style.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  if (style == style_) {
    return;
  }
  style_ = style;
  emit Changed();
}

}  // namespace snapper
