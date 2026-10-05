#ifndef SNAPPER_EDIT_SELECTION_MANAGER_H_
#define SNAPPER_EDIT_SELECTION_MANAGER_H_

#include <QObject>
#include <QString>

#include <cassert>
#include <compare>
#include <set>
#include <vector>

#include "edit/track_ref.h"
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
// shot, and keys. Many can be picked at once; the focus is the one the
// panels show. Picking is not an edit, so it has no undo; after every
// edit or undo anything picked that no longer exists is dropped.
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

  // Focusing another shot drops what was picked inside the old one.
  void SelectShot(ShotId shot);
  void PickShots(const std::set<ShotId>& shots, PickMode mode);
  void SelectLayer(LayerId layer);
  // add keeps the current pick (Shift); otherwise it is replaced.
  void SelectPieces(const std::set<QString>& pieces, bool add);
  // focus becomes the focused layer when it stays picked.
  void PickThings(const std::set<Pick>& picks, PickMode mode,
                  LayerId focus);
  void SelectKeys(const std::set<KeyRef>& keys, bool add);
  void PickKeys(const std::set<KeyRef>& keys, PickMode mode);
  void ClearKeys();
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
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SELECTION_MANAGER_H_
