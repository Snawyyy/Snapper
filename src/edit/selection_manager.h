#ifndef SNAPPER_EDIT_SELECTION_MANAGER_H_
#define SNAPPER_EDIT_SELECTION_MANAGER_H_

#include <QObject>
#include <QString>

#include <cassert>
#include <compare>
#include <optional>
#include <set>
#include <vector>

#include "edit/track_ref.h"
#include "model/reel.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// One picked thing in the current shot: a whole layer (empty piece) or
// one piece of a doll layer.
struct Pick final {
  LayerId layer;
  QString piece;

  auto operator<=>(const Pick&) const = default;
  bool operator==(const Pick&) const = default;
};

// One dot of a picked piece's warp grid, picked so the wheel sets its
// reach and D makes it drag.
struct WarpDot final {
  LayerId layer;
  QString piece;
  int point = -1;

  auto operator<=>(const WarpDot&) const = default;
  bool operator==(const WarpDot&) const = default;
};

// How a pick combines with what is already picked: replace it (click),
// add to it (Shift), flip each thing in or out (Ctrl), or take things
// out (Ctrl+box).
enum class PickMode { kReplace, kAdd, kToggle, kRemove };

// Applies mode to set: the one rule every pickable list shares.
template <typename T>
std::set<T> Combine(const std::set<T>& set, const std::set<T>& items,
                    PickMode mode) {
  assert(set.size() < 10000000);
  assert(items.size() < 10000000);
  std::set<T> out = mode == PickMode::kReplace ? std::set<T>() : set;
  for (const T& item : items) {
    const bool is_in = out.contains(item);
    const bool is_out =
        mode == PickMode::kRemove || (mode == PickMode::kToggle && is_in);
    if (is_out) {
      out.erase(item);
    } else {
      out.insert(item);
    }
  }
  return out;
}

// What the user has picked: shots, layers and pieces in the current
// shot, keys, and clips on the reel. Many can be picked at once; the
// focus is the one the panels show. Picking is not an edit, so it has
// no undo; after every edit or undo anything picked that no longer
// exists is dropped.
class SelectionManager final : public QObject {
  Q_OBJECT

 public:
  explicit SelectionManager(HistoryManager* history);

  // The focused shot, and every picked shot (the focus among them).
  ShotId shot() const { return shot_; }
  const std::set<ShotId>& shots() const { return shots_; }
  // The focused layer; its picked pieces.
  LayerId layer() const { return focus_; }
  std::set<QString> pieces() const;
  // Everything picked in the current shot.
  const std::set<Pick>& picks() const { return picks_; }
  // Picked layers, whole or by piece, without repeats.
  std::vector<LayerId> PickedLayers() const;
  // One channel per picked thing: its piece, or the layer's own move.
  std::vector<TrackRef> PickedTracks() const;
  const std::set<KeyRef>& keys() const { return keys_; }
  const std::set<ClipId>& clips() const { return clips_; }
  // The focused warp dot of the picked ones, while its piece is picked
  // and the dot is still on its grid.
  std::optional<WarpDot> dot() const;
  // Every picked warp dot that is still real, in order.
  std::vector<WarpDot> dots() const;

  // Focusing another shot drops what was picked inside the old one.
  void SelectShot(ShotId shot);
  void PickShots(const std::set<ShotId>& shots, PickMode mode);
  void SelectLayer(LayerId layer);
  // add keeps the current pick (Shift); otherwise it is replaced.
  void SelectPieces(const std::set<QString>& pieces, bool add);
  // focus becomes the focused layer when it stays picked.
  void PickThings(const std::set<Pick>& picks, PickMode mode,
                  LayerId focus);
  // Tab: steps the single pick to the next piece of its doll (direction
  // 1) or the previous (-1), wrapping; a whole layer steps to the next
  // layer. Does nothing with nothing picked.
  void StepPick(int direction);
  void SelectKeys(const std::set<KeyRef>& keys, bool add);
  void PickKeys(const std::set<KeyRef>& keys, PickMode mode);
  void ClearKeys();
  // The frames an edit at frame at goes to, one rule for the stage and
  // every panel: each picked key's frame, or at alone with no key
  // picked.
  std::set<Frame> EditFrames(Frame at) const;
  void PickClips(const std::set<ClipId>& clips, PickMode mode);
  // Picks a warp dot of a picked piece, replacing the dots picked (or
  // none); nothing drops it.
  void PickDot(std::optional<WarpDot> dot);
  // Picks many warp dots at once, combined as mode says; the first of
  // dots takes the focus.
  void PickDots(const std::set<WarpDot>& dots, PickMode mode);
  // Picks a group's dots and, so they can act, their pieces: replace,
  // add (Shift), or flip (Ctrl: a group wholly picked is taken out, else
  // added). Taking out leaves the pieces picked.
  void PickGroup(const std::set<WarpDot>& dots, PickMode mode);
  void Clear();

 signals:
  void Changed();

 private:
  void Prune();
  bool IsReal(const Pick& pick) const;
  bool IsKey(const KeyRef& key) const;
  void FixFocus();

  HistoryManager* history_;
  ShotId shot_;
  std::set<ShotId> shots_;
  LayerId focus_;
  std::set<Pick> picks_;
  std::set<KeyRef> keys_;
  std::set<ClipId> clips_;
  bool IsRealDot(const WarpDot& dot) const;

  std::set<WarpDot> dots_;
  std::optional<WarpDot> dot_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SELECTION_MANAGER_H_
