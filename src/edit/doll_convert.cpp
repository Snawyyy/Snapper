#include "edit/doll_convert.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <set>

#include "anim/doll_pose.h"
#include "anim/sampler.h"
#include "base/text.h"

namespace snapper {
namespace {

constexpr double kDegrees = 180.0 / std::numbers::pi;
// A dial's whole sweep (0 to 1) is this many degrees of its smart bone.
constexpr double kDialSweep = 180.0;

struct Counter final {
  int next = 1;
  int Take() { return next++; }
};

// A piece at rest: its drawing pixels to doll space.
std::map<QString, QTransform> RestPlaces(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(doll.art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  std::map<QString, QTransform> places;
  for (const PlacedPiece& placed : PlaceDoll(doll, PoseMap{})) {
    places[placed.name] = placed.transform;
  }
  return places;
}

// Sets part's rest so its own space lands where transform puts the
// piece's drawing, turning around the joint.
void PlaceLike(const QTransform& transform, QPointF pivot,
               CharacterPart* part) {
  assert(part != nullptr);
  assert(std::isfinite(pivot.x()));
  const double scale_x = std::hypot(transform.m11(), transform.m12());
  const double scale_y = scale_x > 0.0 ? transform.determinant() / scale_x : 1.0;
  part->origin = pivot;
  part->offset = transform.map(pivot) - pivot;
  part->rotation = std::atan2(transform.m12(), transform.m11()) * kDegrees;
  part->scale_x = scale_x > 0.0 ? scale_x : 1.0;
  part->scale_y = scale_y;
}

CharacterPart Picture(const ArtPiece& art, int drawing, const QString& name,
                      PartId id, PartId parent) {
  assert(drawing >= 0 && static_cast<size_t>(drawing) < art.drawings.size());
  assert(id.IsValid());
  CharacterPart part;
  part.id = id;
  part.name = name;
  part.kind = PartKind::kImage;
  part.parent = parent;
  part.bind = BindMode::kNone;
  part.image = art.drawings[static_cast<size_t>(drawing)];
  part.image_size = QSizeF(art.size);
  return part;
}

// The bones, one per rig piece, from the joint toward the children.
void AddBones(const Doll& doll, const std::map<QString, QTransform>& places,
              Counter* ids, CharacterPart* skeleton, DollParts* out) {
  assert(skeleton != nullptr && out != nullptr);
  assert(ids != nullptr);
  for (const RigPiece& piece : doll.rig.pieces) {
    out->bones[piece.name] = BoneId(ids->Take());
  }
  for (const RigPiece& piece : doll.rig.pieces) {
    const auto place = places.find(piece.name);
    const QTransform transform =
        place == places.end() ? QTransform() : place->second;
    const QPointF base = transform.map(piece.pivot);
    QPointF tip;
    int children = 0;
    for (const RigPiece& child : doll.rig.pieces) {
      const auto child_place = places.find(child.name);
      if (child.parent == piece.name && child_place != places.end()) {
        tip += child_place->second.map(child.pivot);
        ++children;
      }
    }
    if (children > 0) {
      tip /= children;
    } else {
      const QSize size = ArtSizeOf(doll, piece.name);
      const QPointF middle(size.width() / 2.0, size.height() / 2.0);
      tip = transform.map(piece.pivot + 2.0 * (middle - piece.pivot));
    }
    if (QLineF(base, tip).length() < 10.0) {
      tip = base + QPointF(0.0, 40.0);
    }
    Bone bone;
    bone.id = out->bones[piece.name];
    bone.name = piece.name;
    bone.parent = piece.parent.isEmpty() ? BoneId() : out->bones[piece.parent];
    bone.base = base;
    bone.length = QLineF(base, tip).length();
    bone.angle = std::atan2(tip.y() - base.y(), tip.x() - base.x()) * kDegrees;
    bone.strength = 0.0;
    skeleton->bones.push_back(bone);
  }
}

// A piece's pose as bone turn and push, part scale, opacity and pick.
void AddPiecePose(const QString& piece, const PiecePose& pose,
                  const DollParts& parts, CharacterPose* out) {
  assert(out != nullptr);
  assert(!piece.isEmpty());
  const auto bone = parts.bones.find(piece);
  const auto part = parts.parts.find(piece);
  if (bone == parts.bones.end() || part == parts.parts.end()) {
    return;
  }
  BonePose& turned = out->bones[bone->second];
  turned.turn += pose.rotation;
  turned.offset += pose.offset;
  PartPose& shown = out->parts[part->second];
  shown.scale_x *= pose.scale_x;
  shown.scale_y *= pose.scale_y;
  shown.opacity *= pose.opacity;
  shown.order += pose.order;
  const auto drawings = parts.drawings.find(piece);
  const bool picks = drawings != parts.drawings.end() && pose.drawing >= 0 &&
                     static_cast<size_t>(pose.drawing) < drawings->second.size();
  if (picks) {
    shown.pick = drawings->second[static_cast<size_t>(pose.drawing)];
  }
}

void AddDials(const Doll& doll, Counter* ids, CharacterPart* skeleton,
              Character* character, DollParts* out) {
  assert(skeleton != nullptr && character != nullptr);
  assert(out != nullptr);
  const QPointF corner(doll.art.canvas.width() / 2.0 + 60.0,
                       -doll.art.canvas.height() / 2.0 + 40.0);
  for (size_t i = 0; i < doll.rig.dials.size(); ++i) {
    const SmartDial& dial = doll.rig.dials[i];
    Bone bone;
    bone.id = BoneId(ids->Take());
    bone.name = dial.name;
    bone.base = corner + QPointF(0.0, 50.0 * static_cast<double>(i));
    bone.length = 40.0;
    bone.is_smart = true;
    skeleton->bones.push_back(bone);
    out->dials[dial.name] = bone.id;
    const double rest = RestValue(dial);
    out->dial_rests[dial.name] = rest;
    SmartAction action{bone.id, dial.name, {}};
    for (const DialStop& stop : dial.stops) {
      const double turn = (stop.at - rest) * kDialSweep;
      if (std::abs(turn) < 0.5) {
        continue;
      }
      CharacterPose pose;
      for (const auto& [piece, piece_pose] : stop.poses) {
        PiecePose added = piece_pose;
        added.drawing = -1;
        AddPiecePose(piece, added, *out, &pose);
      }
      for (const auto& [piece, drawing] : stop.drawings) {
        const ArtPiece* art = FindArt(doll, piece);
        const auto children = out->drawings.find(piece);
        const auto part = out->parts.find(piece);
        const int index = art != nullptr ? FindDrawing(*art, drawing) : -1;
        if (children != out->drawings.end() && part != out->parts.end() &&
            index >= 0 && static_cast<size_t>(index) < children->second.size()) {
          pose.parts[part->second].pick =
              children->second[static_cast<size_t>(index)];
        }
      }
      action.stops.push_back({turn, pose});
    }
    std::ranges::sort(action.stops, {}, &ActionStop::turn);
    character->actions.push_back(std::move(action));
  }
}

}  // namespace

Character CharacterFromDoll(const Doll& doll, DollParts* parts) {
  assert(parts != nullptr);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  Counter ids;
  Character character;
  character.name = doll.name;
  character.folder = doll.folder;
  CharacterPart skeleton;
  skeleton.id = PartId(ids.Take());
  skeleton.name = Tr("Bones");
  skeleton.kind = PartKind::kBone;
  const std::map<QString, QTransform> places = RestPlaces(doll);
  AddBones(doll, places, &ids, &skeleton, parts);
  character.parts.push_back(skeleton);
  std::vector<const RigPiece*> in_order;
  for (const RigPiece& piece : doll.rig.pieces) in_order.push_back(&piece);
  std::ranges::stable_sort(in_order, {}, &RigPiece::order);
  for (const RigPiece* piece : in_order) {
    const ArtPiece* art = FindArt(doll, piece->name);
    const auto place = places.find(piece->name);
    if (art == nullptr || art->drawings.empty() || place == places.end()) {
      continue;
    }
    const PartId id(ids.Take());
    parts->parts[piece->name] = id;
    const int shown = std::max(0, ShownDrawingIndex(doll, piece->name, -1));
    if (art->drawings.size() == 1) {
      CharacterPart picture = Picture(*art, 0, piece->name, id, skeleton.id);
      PlaceLike(place->second, piece->pivot, &picture);
      picture.bind = BindMode::kLayer;
      picture.bound_bone = parts->bones[piece->name];
      character.parts.push_back(picture);
      continue;
    }
    CharacterPart chooser;
    chooser.id = id;
    chooser.name = piece->name;
    chooser.kind = PartKind::kSwitch;
    chooser.parent = skeleton.id;
    chooser.bind = BindMode::kLayer;
    chooser.bound_bone = parts->bones[piece->name];
    std::vector<CharacterPart> pictures;
    for (size_t d = 0; d < art->drawings.size(); ++d) {
      const int index = static_cast<int>(d);
      CharacterPart picture = Picture(*art, index, DrawingName(*art, index),
                                      PartId(ids.Take()), id);
      PlaceLike(place->second, piece->pivot, &picture);
      parts->drawings[piece->name].push_back(picture.id);
      pictures.push_back(std::move(picture));
    }
    chooser.rest_pick = parts->drawings[piece->name][static_cast<size_t>(
        std::min<int>(shown, static_cast<int>(pictures.size()) - 1))];
    character.parts.push_back(chooser);
    character.parts.insert(character.parts.end(), pictures.begin(),
                           pictures.end());
  }
  AddDials(doll, &ids, &character.parts.front(), &character, parts);
  character.next_id = ids.next;
  assert(IsSound(character));
  return character;
}

CharacterLayer CharacterLayerFromDoll(const Doll& doll, const DollLayer& layer,
                                      const DollParts& parts) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(layer.pieces.size() <= static_cast<size_t>(kMaxPieceTracks));
  CharacterLayer out{layer.doll, {}, layer.is_flipped};
  std::map<Frame, Ease> frames;
  for (const auto& [piece, channel] : layer.pieces) {
    for (const Key<PiecePose>& key : channel.keys) {
      frames.try_emplace(key.frame, key.ease);
    }
  }
  for (const auto& [dial, channel] : layer.dials) {
    for (const Key<double>& key : channel.keys) {
      frames.try_emplace(key.frame, key.ease);
    }
  }
  for (const auto& [frame, ease] : frames) {
    CharacterPose pose;
    for (const auto& [piece, channel] : layer.pieces) {
      AddPiecePose(piece, Sample(channel, frame, PiecePose()), parts, &pose);
    }
    for (const auto& [dial, channel] : layer.dials) {
      const auto bone = parts.dials.find(dial);
      const auto rest = parts.dial_rests.find(dial);
      if (bone != parts.dials.end() && rest != parts.dial_rests.end()) {
        pose.bones[bone->second].turn =
            (Sample(channel, frame, rest->second) - rest->second) * kDialSweep;
      }
    }
    static_cast<void>(SetKey(&out.poses, Key<CharacterPose>{frame, pose, ease}));
  }
  return out;
}

Project WithDollsAsCharacters(Project project) {
  assert(project.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  std::map<QString, DollParts> converted;
  for (const auto& [name, doll] : project.dolls) {
    const bool is_taken = project.characters.contains(name);
    const bool is_full =
        project.characters.size() >= static_cast<size_t>(kMaxProjectCharacters);
    if (doll == nullptr || doll->rig.object.is_object || is_taken || is_full) {
      continue;
    }
    DollParts parts;
    Character character = CharacterFromDoll(*doll, &parts);
    character.name = name;
    project.characters[name] =
        std::make_shared<const Character>(std::move(character));
    converted[name] = std::move(parts);
  }
  if (converted.empty()) {
    return project;
  }
  for (auto& shared : project.shots) {
    Shot shot = *shared;
    for (Layer& layer : shot.layers) {
      const auto* posed = std::get_if<DollLayer>(&layer.content);
      const auto parts =
          posed != nullptr ? converted.find(posed->doll) : converted.end();
      if (parts == converted.end()) {
        continue;
      }
      layer.content = CharacterLayerFromDoll(*project.dolls.at(posed->doll),
                                             *posed, parts->second);
    }
    shared = std::make_shared<const Shot>(std::move(shot));
  }
  for (const auto& [name, parts] : converted) {
    project.dolls.erase(name);
  }
  return project;
}

}  // namespace snapper
