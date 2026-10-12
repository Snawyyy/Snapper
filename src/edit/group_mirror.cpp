#include "edit/group_mirror.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>

#include "anim/mirror.h"
#include "anim/warp_shape.h"
#include "base/text.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

// For each copied piece, which dot of its twin each of its dots pairs
// with (-1 for none).
using Pairs = std::map<QString, std::vector<int>>;

Pairs PairsOf(const Doll& before, const Doll& after,
              const std::vector<QString>& copied) {
  assert(copied.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(before.rig.pieces.size() == after.rig.pieces.size());
  Pairs pairs;
  for (const QString& name : copied) {
    const RigPiece* from = FindRig(before.rig, name);
    const RigPiece* to = FindRig(after.rig, MirrorName(name));
    const bool is_twin = from != nullptr && to != nullptr;
    if (is_twin) {
      pairs[name] = MirroredPoints(from->warp, ArtSizeOf(before, name),
                                   to->warp, ArtSizeOf(after, to->name));
    }
  }
  return pairs;
}

// The side the copied pieces under group's dots are on: -1 left, 1
// right, 0 when none of them has a side.
int SideOfDots(const DotGroup& group, const Pairs& pairs) {
  assert(group.dots.size() <= static_cast<size_t>(kMaxGroupDots));
  for (const GroupDot& dot : group.dots) {
    const int side = pairs.contains(dot.piece) ? SideOf(dot.piece) : 0;
    if (side != 0) {
      return side;
    }
  }
  return 0;
}

// name marked with side the way pieces are: "hair" on the left is
// "hair_L".
QString Sided(const QString& name, int side) {
  assert(side == -1 || side == 1);
  assert(!name.isEmpty());
  return name + (side < 0 ? QStringLiteral("_L") : QStringLiteral("_R"));
}

// group's dots that sit on a copied piece (of this doll's layers, when
// it has layers), as the dots of the twin piece.
std::vector<GroupDot> TwinDots(const DotGroup& group, const Pairs& pairs,
                               const std::set<LayerId>& layers) {
  assert(group.dots.size() <= static_cast<size_t>(kMaxGroupDots));
  assert(layers.size() < 100000);
  std::vector<GroupDot> twin;
  for (const GroupDot& dot : group.dots) {
    const auto found = pairs.find(dot.piece);
    const bool is_ours = !dot.layer.IsValid() || layers.contains(dot.layer);
    const bool is_in_range =
        found != pairs.end() && dot.point >= 0 &&
        static_cast<size_t>(dot.point) < found->second.size();
    const int point =
        is_ours && is_in_range
            ? found->second[static_cast<size_t>(dot.point)]
            : -1;
    const GroupDot made{dot.layer, MirrorName(dot.piece), point};
    const bool is_new =
        point >= 0 && std::ranges::find(twin, made) == twin.end();
    if (is_new) {
      twin.push_back(made);
    }
  }
  return twin;
}

// groups with a twin of every group that has dots on a copied piece,
// remaking a twin that exists. A group without a side in its name takes
// the side of its pieces first ("hair" becomes "hair_L"), so its twin is
// "hair_R" and copying again only remakes that one twin.
void AddTwins(const Pairs& pairs, const std::set<LayerId>& layers,
              std::vector<DotGroup>* groups) {
  assert(groups != nullptr);
  assert(groups->size() <= static_cast<size_t>(kMaxDotGroups));
  const std::vector<DotGroup> originals = *groups;
  for (const DotGroup& group : originals) {
    std::vector<GroupDot> dots = TwinDots(group, pairs, layers);
    if (dots.empty()) {
      continue;
    }
    QString own = group.name;
    const int side = SideOfDots(group, pairs);
    const bool needs_side = SideOf(own) == 0 && side != 0 &&
                            FindGroup(*groups, Sided(own, side)) == nullptr;
    if (needs_side) {
      own = Sided(own, side);
      FindGroup(groups, group.name)->name = own;
    }
    const QString name = MirrorName(own);
    // A group with no side on a piece with none has no twin to make.
    if (name == own) {
      continue;
    }
    DotGroup* existing = FindGroup(groups, name);
    const bool is_room =
        existing != nullptr ||
        groups->size() < static_cast<size_t>(kMaxDotGroups);
    if (existing != nullptr) {
      existing->dots = std::move(dots);
    } else if (is_room) {
      groups->push_back({name, std::move(dots)});
    }
  }
}

}  // namespace

Project WithTwinGroups(const Project& project, const QString& doll,
                       const Doll& before, Doll after,
                       const std::vector<QString>& copied) {
  assert(!doll.isEmpty());
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const Pairs pairs = PairsOf(before, after, copied);
  Project next = project;
  for (auto& shared : next.shots) {
    std::set<LayerId> layers;
    for (const Layer& layer : shared->layers) {
      const DollLayer* posed = PosedLayerOf(project, shared->id, layer.id);
      const bool is_ours = posed != nullptr && posed->doll == doll;
      if (is_ours) {
        layers.insert(layer.id);
      }
    }
    const bool has_layers = !layers.empty();
    if (has_layers) {
      Shot shot = *shared;
      AddTwins(pairs, layers, &shot.dot_groups);
      shared = std::make_shared<const Shot>(std::move(shot));
    }
  }
  AddTwins(pairs, {}, &after.rig.dot_groups);
  next.dolls[doll] = std::make_shared<const Doll>(std::move(after));
  return next;
}

}  // namespace snapper
