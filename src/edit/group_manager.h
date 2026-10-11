#ifndef SNAPPER_EDIT_GROUP_MANAGER_H_
#define SNAPPER_EDIT_GROUP_MANAGER_H_

#include <QString>

#include <set>
#include <vector>

#include "base/error.h"
#include "edit/group_edits.h"
#include "edit/selection_manager.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// Named sets of warp dots, so a set that must stay still and a set that
// must be jelly are each picked with one click. A group belongs to its
// shot; a default is kept on the doll and every shot shows it. What a
// group holds is data; the picking is SelectionManager's and the
// material is RigManager's.
class GroupManager final {
 public:
  explicit GroupManager(HistoryManager* history);

  // What the shot shows, its own groups first.
  std::vector<ShownGroup> GroupsOf(ShotId shot) const;
  // Why dots can't become a group now; empty when they can.
  QString WhyNoGroup(ShotId shot, const std::set<WarpDot>& dots) const;

  // The name a new group gets unless the user picks one: "Group 1", or
  // the first number free.
  QString SuggestedName(ShotId shot) const;

  // Makes dots a group called name (the suggested name when it is
  // empty) and returns the name.
  Result<QString> Create(ShotId shot, const std::set<WarpDot>& dots,
                         const QString& name = QString());
  Result<void> Rename(ShotId shot, const QString& name, const QString& to);
  Result<void> Delete(ShotId shot, const QString& name);
  // A group left with no dots is deleted.
  Result<void> AddDots(ShotId shot, const QString& name,
                       const std::set<WarpDot>& dots);
  Result<void> RemoveDots(ShotId shot, const QString& name,
                          const std::set<WarpDot>& dots);
  // A default is kept on the dolls and shown by every shot; off makes it
  // this shot's own and takes it from the dolls on the shot's layers.
  Result<void> SetDefault(ShotId shot, const QString& name,
                          bool is_default);

 private:
  Result<ShownGroup> Find(ShotId shot, const QString& name) const;
  Result<void> Write(const QString& label, ShotId shot, const QString& name,
                     const std::optional<ShownGroup>& group);

  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_GROUP_MANAGER_H_
