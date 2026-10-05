#ifndef SNAPPER_EDIT_STAGE_MANAGER_H_
#define SNAPPER_EDIT_STAGE_MANAGER_H_

#include <QColor>
#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/layer.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

QString EffectName(EffectKind kind);

// What stands on a shot's stage: its layers, their order and their
// settings. Poses and keys belong to PoseManager and KeyManager.
class StageManager final {
 public:
  explicit StageManager(HistoryManager* history);

  // New layers go on top, centred, and show for the whole shot.
  Result<LayerId> AddDoll(ShotId shot, const QString& doll);
  Result<LayerId> AddImage(ShotId shot, const QString& path);
  Result<LayerId> AddText(ShotId shot, const QString& text);
  Result<LayerId> AddEffect(ShotId shot, EffectKind kind);
  // A copy just above the original.
  Result<LayerId> Duplicate(ShotId shot, LayerId layer);
  Result<void> Remove(ShotId shot, LayerId layer);
  // 0 is the bottom.
  Result<void> Move(ShotId shot, LayerId layer, int index);

  Result<void> Rename(ShotId shot, LayerId layer, const QString& name);
  Result<void> SetVisible(ShotId shot, LayerId layer, bool is_visible);
  // length 0 shows it to the end of the shot.
  Result<void> SetRange(ShotId shot, LayerId layer, Frame start,
                        Frame length);
  Result<void> SetFlipped(ShotId shot, LayerId layer, bool is_flipped);
  Result<void> SetText(ShotId shot, LayerId layer, const TextLayer& text);
  Result<void> SetEffect(ShotId shot, LayerId layer, EffectKind kind,
                         QColor color);

  // The same change to many layers at once, as one undo step. Numbers
  // are added to each layer's own value, so they keep their differences.
  Result<std::vector<LayerId>> DuplicateAll(ShotId shot,
                                            const std::vector<LayerId>& ids);
  Result<void> RemoveAll(ShotId shot, const std::vector<LayerId>& ids);
  Result<void> ShowAll(ShotId shot, const std::vector<LayerId>& ids,
                       bool is_visible);
  Result<void> FlipAll(ShotId shot, const std::vector<LayerId>& ids,
                       bool is_flipped);
  // Moves each picked layer one place up (step 1) or down (-1), keeping
  // their order among themselves.
  Result<void> Restack(ShotId shot, const std::vector<LayerId>& ids,
                       int step);
  Result<void> ShiftTiming(ShotId shot, const std::vector<LayerId>& ids,
                           int start_delta, int length_delta);
  Result<void> ShiftText(ShotId shot, const std::vector<LayerId>& ids,
                         double size_delta, double outline_delta);
  // Bold and colours are set, not added, on every text layer picked.
  Result<void> StyleText(ShotId shot, const std::vector<LayerId>& ids,
                         bool is_bold, QColor fill, QColor outline);
  Result<void> SetEffectAll(ShotId shot, const std::vector<LayerId>& ids,
                            EffectKind kind, QColor color);

 private:
  Result<LayerId> AddLayer(ShotId shot, const QString& name,
                           LayerContent content, const QString& label);

  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_STAGE_MANAGER_H_
