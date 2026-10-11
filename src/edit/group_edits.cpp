#include "edit/group_edits.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "base/text.h"
#include "edit/project_edits.h"
#include "edit/rig_edits.h"

namespace snapper {
namespace {

// True when piece of doll has a warp dot called point.
bool IsOn(const Doll& doll, const QString& piece, int point) {
  assert(point >= -1);
  assert(piece.size() < 100000);
  const RigPiece* rig = FindRig(doll.rig, piece);
  return rig != nullptr && point >= 0 && point < rig->warp.PointCount();
}

// The names of the dolls on shot's layers, each once.
std::vector<QString> DollsOf(const Project& project, const Shot& shot) {
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  assert(shot.id.IsValid());
  std::vector<QString> names;
  for (const Layer& layer : shot.layers) {
    const DollLayer* posed = PosedLayerOf(project, shot.id, layer.id);
    const bool is_new = posed != nullptr &&
                        std::ranges::find(names, posed->doll) == names.end();
    if (is_new) {
      names.push_back(posed->doll);
    }
  }
  return names;
}

// The dots of group that are on doll's layers, with no layer: how a
// doll keeps a default.
std::vector<GroupDot> DefaultDotsOf(const Project& project, ShotId shot,
                                    const QString& doll,
                                    const std::set<WarpDot>& dots) {
  assert(shot.IsValid());
  assert(dots.size() <= static_cast<size_t>(kMaxGroupDots));
  std::vector<GroupDot> kept;
  for (const WarpDot& dot : dots) {
    const DollLayer* on = PosedLayerOf(project, shot, dot.layer);
    const GroupDot plain{LayerId(), dot.piece, dot.point};
    const bool is_new = on != nullptr && on->doll == doll &&
                        std::ranges::find(kept, plain) == kept.end();
    if (is_new) {
      kept.push_back(plain);
    }
  }
  return kept;
}

// The shot's own groups, as shown.
std::vector<ShownGroup> OwnGroupsOf(const Project& project, const Shot& shot) {
  assert(shot.id.IsValid());
  assert(shot.dot_groups.size() <= static_cast<size_t>(kMaxDotGroups));
  std::vector<ShownGroup> shown;
  for (const DotGroup& group : shot.dot_groups) {
    ShownGroup out{group.name, {}, false};
    for (const GroupDot& dot : group.dots) {
      const Doll* doll = dot.layer.IsValid()
                             ? DollOfLayer(project, shot.id, dot.layer)
                             : nullptr;
      const bool is_there =
          doll != nullptr && IsOn(*doll, dot.piece, dot.point);
      if (is_there) {
        out.dots.insert({dot.layer, dot.piece, dot.point});
      }
    }
    shown.push_back(std::move(out));
  }
  return shown;
}

// Adds the default groups of layer's doll to shown, past the first own
// groups (which hide a default of the same name).
void AddDefaultsOf(const Doll& doll, LayerId layer, size_t own,
                   std::vector<ShownGroup>* shown) {
  assert(shown != nullptr);
  assert(own <= shown->size());
  for (const DotGroup& group : doll.rig.dot_groups) {
    const auto own_end = shown->begin() + static_cast<std::ptrdiff_t>(own);
    const auto is_named = [&group](const ShownGroup& s) {
      return s.name == group.name;
    };
    const bool is_hidden =
        std::any_of(shown->begin(), own_end, is_named);
    auto found = std::find_if(own_end, shown->end(), is_named);
    const bool is_new = !is_hidden && found == shown->end();
    if (is_new) {
      shown->push_back({group.name, {}, true});
      found = shown->end() - 1;
    }
    for (const GroupDot& dot : group.dots) {
      const bool is_there = !is_hidden && IsOn(doll, dot.piece, dot.point);
      if (is_there) {
        found->dots.insert({layer, dot.piece, dot.point});
      }
    }
  }
}

// Why group may not be written in place of old_name; empty when it may.
QString WhyNotWritten(const std::vector<ShownGroup>& shown,
                      const QString& old_name,
                      const std::optional<ShownGroup>& group) {
  assert(shown.size() <= static_cast<size_t>(2 * kMaxDotGroups * 8));
  assert(old_name.size() < 100000);
  const bool is_clash =
      group && group->name != old_name &&
      std::ranges::any_of(shown, [&group](const ShownGroup& s) {
        return s.name == group->name;
      });
  if (is_clash) {
    return Tr("There is already a group called %1.").arg(group->name);
  }
  const bool is_big =
      group && group->dots.size() > static_cast<size_t>(kMaxGroupDots);
  return is_big ? Tr("A group holds at most %1 dots.").arg(kMaxGroupDots)
                : QString();
}

// Puts group where old_name was in groups (or last), or only takes
// old_name out when there is no group. False when there is no room.
bool PutGroup(std::vector<DotGroup>* groups, const QString& old_name,
              std::optional<DotGroup> group) {
  assert(groups != nullptr);
  assert(groups->size() <= static_cast<size_t>(kMaxDotGroups));
  const auto old = std::ranges::find_if(
      *groups, [&old_name](const DotGroup& g) { return g.name == old_name; });
  const bool has_old = old != groups->end();
  const bool is_room =
      has_old || groups->size() < static_cast<size_t>(kMaxDotGroups);
  if (!group) {
    if (has_old) {
      groups->erase(old);
    }
    return true;
  }
  if (!is_room) {
    return false;
  }
  if (has_old) {
    *old = std::move(*group);
  } else {
    groups->push_back(std::move(*group));
  }
  return true;
}

// doll's rig with old_name gone from its defaults and group, if it is a
// default with dots of this doll, added.
Result<void> RewriteDefaults(const Project& project, ShotId id,
                             const QString& doll, const QString& old_name,
                             const std::optional<ShownGroup>& group,
                             Rig* rig) {
  assert(rig != nullptr);
  assert(id.IsValid());
  const bool is_default = group && group->is_default;
  std::vector<GroupDot> dots;
  if (is_default) {
    dots = DefaultDotsOf(project, id, doll, group->dots);
  }
  const bool has_dots = !dots.empty();
  // A group made a default takes the place of a default it hid.
  const QString at = old_name.isEmpty() && is_default ? group->name : old_name;
  const bool is_put = PutGroup(
      &rig->dot_groups, at,
      has_dots ? std::optional<DotGroup>(DotGroup{group->name, dots})
               : std::nullopt);
  if (!is_put) {
    return std::unexpected(Error{Tr("%1 holds at most %2 default groups.")
                                     .arg(doll)
                                     .arg(kMaxDotGroups)});
  }
  return Result<void>();
}

// The project with every doll on shot's layers rewritten by
// RewriteDefaults.
Result<Project> WithDefaultsRewritten(
    const Project& project, const Shot& shot, const QString& old_name,
    const std::optional<ShownGroup>& group) {
  assert(shot.id.IsValid());
  assert(old_name.size() < 100000);
  Project next = project;
  for (const QString& doll : DollsOf(project, shot)) {
    Result<Project> done =
        WithRig(next, doll, [&](Rig* rig) -> Result<void> {
          return RewriteDefaults(project, shot.id, doll, old_name, group,
                                 rig);
        });
    const bool is_failed = !done;
    if (is_failed) {
      return done;
    }
    next = *std::move(done);
  }
  return next;
}

}  // namespace

std::vector<ShownGroup> ShownGroupsOf(const Project& project, ShotId id) {
  assert(id.value() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const Shot* shot = FindShot(project, id);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return {};
  }
  std::vector<ShownGroup> shown = OwnGroupsOf(project, *shot);
  const size_t own = shown.size();
  for (const Layer& layer : shot->layers) {
    const Doll* doll = DollOfLayer(project, id, layer.id);
    const bool has_doll = doll != nullptr;
    if (has_doll) {
      AddDefaultsOf(*doll, layer.id, own, &shown);
    }
  }
  return shown;
}

Result<Project> WithGroupWritten(const Project& project, ShotId id,
                                 const QString& old_name,
                                 const std::optional<ShownGroup>& group) {
  assert(id.value() >= 0);
  assert(old_name.size() < 100000);
  const Shot* shot = FindShot(project, id);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return std::unexpected(Error{Tr("That shot no longer exists.")});
  }
  const std::vector<ShownGroup> shown = ShownGroupsOf(project, id);
  const QString why_not = WhyNotWritten(shown, old_name, group);
  const bool can_write = why_not.isEmpty();
  if (!can_write) {
    return std::unexpected(Error{why_not});
  }
  const bool is_default = group && group->is_default;
  // Only the place the old group came from gives it up: a shot's group
  // may hide a default of the same name, which stays.
  const bool was_default = std::ranges::any_of(
      shown, [&old_name](const ShownGroup& s) {
        return s.name == old_name && s.is_default;
      });
  const QString from_shot = was_default ? QString() : old_name;
  Result<Project> next = WithDefaultsRewritten(
      project, *shot, was_default ? old_name : QString(), group);
  const bool is_failed = !next;
  if (is_failed) {
    return next;
  }
  return WithShot(*std::move(next), id, [&](Shot* edited) -> Result<void> {
    const bool is_shot_group = group && !is_default;
    std::optional<DotGroup> out;
    if (is_shot_group) {
      out = DotGroup{group->name, {}};
      for (const WarpDot& dot : group->dots) {
        out->dots.push_back({dot.layer, dot.piece, dot.point});
      }
    }
    const bool is_put = PutGroup(&edited->dot_groups, from_shot, out);
    if (!is_put) {
      return std::unexpected(
          Error{Tr("A shot holds at most %1 groups.").arg(kMaxDotGroups)});
    }
    return Result<void>();
  });
}

}  // namespace snapper
