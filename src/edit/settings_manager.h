#ifndef SNAPPER_EDIT_SETTINGS_MANAGER_H_
#define SNAPPER_EDIT_SETTINGS_MANAGER_H_

#include <QColor>
#include <QString>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "edit/selection_manager.h"
#include "model/doll.h"
#include "model/layer.h"
#include "model/project.h"

namespace snapper {

class HistoryManager;

// Copy and paste of a picked part's settings: everything but its pose at
// the playhead. For a piece of a doll that is its warp dots (drag nodes,
// motions, reach); for a whole doll the flip and every piece's dots; for
// text the look of the letters; for an effect its kind and colour. Pasted
// onto something of the same kind it is one undo step; dots need the same
// warp grid. When the target is the same object (the same piece of the
// same doll) the groups of its dots and the drawing it shows come too.
class SettingsManager final {
 public:
  explicit SettingsManager(HistoryManager* history);

  // Copies the settings of the first of picks, with the drawing shown at
  // local. Refused when nothing is picked.
  Result<void> Copy(ShotId shot, const std::set<Pick>& picks, Frame local);
  // Pastes onto every pick of the kind that was copied, as one step.
  Result<void> Paste(ShotId shot, const std::set<Pick>& picks, Frame local);
  // Why Copy or Paste can't act, for their tooltips; empty when they can.
  QString WhyNoCopy(ShotId shot, const std::set<Pick>& picks) const;
  QString WhyNoPaste(ShotId shot, const std::set<Pick>& picks) const;
  bool HasClip() const { return clip_.has_value(); }

 private:
  // One group of the copied dots: its name and its dots on the copied
  // piece or doll.
  struct GroupClip final {
    QString name;
    std::vector<std::pair<QString, int>> dots;
  };
  // One piece of the copy.
  struct PieceClip final {
    WarpGrid warp;
    PieceDots dots;
    int drawing = -1;
  };
  struct Clip final {
    // The doll the settings came from; empty for text and effects.
    QString doll;
    // The whole layer was copied, not one piece of it.
    bool is_whole = false;
    bool is_flipped = false;
    std::map<QString, PieceClip> pieces;
    std::vector<GroupClip> groups;
    std::optional<TextLayer> text;
    std::optional<std::pair<EffectKind, QColor>> effect;
  };

  // What the first pick holds, as a clip; nothing when it holds none.
  static std::optional<Clip> Capture(const Project& project, ShotId shot,
                                     const Pick& pick, Frame local);
  // The project with clip put on target, as the rules above say.
  static Result<Project> PasteOnto(Project project, ShotId shot,
                                   const Clip& clip, const Pick& target,
                                   Frame local);
  static Result<Project> PastePieces(Project project, ShotId shot,
                                     const Clip& clip, const Pick& target,
                                     Frame local);
  static Result<Project> PasteGroups(Project project, ShotId shot,
                                     const Clip& clip, const Pick& target);

  HistoryManager* history_;
  std::optional<Clip> clip_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SETTINGS_MANAGER_H_
