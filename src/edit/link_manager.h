#ifndef SNAPPER_EDIT_LINK_MANAGER_H_
#define SNAPPER_EDIT_LINK_MANAGER_H_

#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// Links on a shot's stage: which things copy another thing's movement,
// from when and how strongly. What a link does is LinkSolver's; this
// says what may be linked and makes the changes.
class LinkManager final {
 public:
  explicit LinkManager(HistoryManager* history);

  // Each of followers copies leader's movement from frame from on, at
  // full strength, replacing a link it had. All or none: one follower
  // that can't follow refuses the lot.
  Result<void> LinkTo(ShotId shot, const std::vector<LinkEnd>& followers,
                      const LinkEnd& leader, Frame from);
  // Followers without a link are skipped.
  Result<void> Unlink(ShotId shot, const std::vector<LinkEnd>& followers);
  // Adds delta to each follower's strength, kept from 0 to
  // kMaxLinkStrength, so many picks keep their differences.
  Result<void> ShiftStrength(ShotId shot,
                             const std::vector<LinkEnd>& followers,
                             double delta);

  // Why each can't act now, for greyed-out controls; empty when it can.
  QString WhyNoLink(ShotId shot, const std::vector<LinkEnd>& followers,
                    const LinkEnd& leader) const;
  QString WhyNoUnlink(ShotId shot,
                      const std::vector<LinkEnd>& followers) const;
  // How end is named to the user: "Box", or "Bob's arm" for a piece.
  QString NameOf(ShotId shot, const LinkEnd& end) const;

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_LINK_MANAGER_H_
