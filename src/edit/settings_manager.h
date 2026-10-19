#ifndef SNAPPER_EDIT_SETTINGS_MANAGER_H_
#define SNAPPER_EDIT_SETTINGS_MANAGER_H_

#include <QColor>
#include <QString>

#include <optional>
#include <set>
#include <utility>

#include "base/error.h"
#include "edit/selection_manager.h"
#include "model/layer.h"
#include "model/project.h"

namespace snapper {

class HistoryManager;

// Copy and paste of a picked layer's settings: everything but its pose
// at the playhead. For text the look of the letters; for an effect its
// kind and colour; for a character its flip. Pasted onto layers of the
// same kind it is one undo step.
class SettingsManager final {
 public:
  explicit SettingsManager(HistoryManager* history);

  // Copies the settings of the first of picks. Refused when nothing is
  // picked.
  Result<void> Copy(ShotId shot, const std::set<Pick>& picks);
  // Pastes onto every pick of the kind that was copied, as one step.
  Result<void> Paste(ShotId shot, const std::set<Pick>& picks);
  // Why Copy or Paste can't act, for their tooltips; empty when they can.
  QString WhyNoCopy(ShotId shot, const std::set<Pick>& picks) const;
  QString WhyNoPaste(ShotId shot, const std::set<Pick>& picks) const;
  bool HasClip() const { return clip_.has_value(); }

 private:
  struct Clip final {
    std::optional<TextLayer> text;
    std::optional<std::pair<EffectKind, QColor>> effect;
    std::optional<bool> is_flipped;
  };

  // What the first pick holds, as a clip; nothing when it holds none.
  static std::optional<Clip> Capture(const Project& project, ShotId shot,
                                     const Pick& pick);
  // The project with clip put on target.
  static Result<Project> PasteOnto(Project project, ShotId shot,
                                   const Clip& clip, const Pick& target);

  HistoryManager* history_;
  std::optional<Clip> clip_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SETTINGS_MANAGER_H_
