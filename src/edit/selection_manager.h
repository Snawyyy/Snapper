#ifndef SNAPPER_EDIT_SELECTION_MANAGER_H_
#define SNAPPER_EDIT_SELECTION_MANAGER_H_

#include <QObject>
#include <QString>

#include <set>

#include "edit/track_ref.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// What the user has picked: a shot, a layer in it, pieces of that
// layer's doll, and keys. Picking is not an edit, so it has no undo;
// after every edit or undo anything picked that no longer exists is
// dropped, so tools never act on something gone.
class SelectionManager final : public QObject {
  Q_OBJECT

 public:
  explicit SelectionManager(HistoryManager* history);

  ShotId shot() const { return shot_; }
  LayerId layer() const { return layer_; }
  const std::set<QString>& pieces() const { return pieces_; }
  const std::set<KeyRef>& keys() const { return keys_; }

  // Picking another shot or layer drops what was picked inside the old
  // one.
  void SelectShot(ShotId shot);
  void SelectLayer(LayerId layer);
  // add keeps the current pick (Shift); otherwise it is replaced.
  void SelectPieces(const std::set<QString>& pieces, bool add);
  void SelectKeys(const std::set<KeyRef>& keys, bool add);
  void ClearKeys();
  void Clear();

 signals:
  void Changed();

 private:
  void Prune();
  bool IsPiece(const QString& piece) const;
  bool IsKey(const KeyRef& key) const;

  HistoryManager* history_;
  ShotId shot_;
  LayerId layer_;
  std::set<QString> pieces_;
  std::set<KeyRef> keys_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SELECTION_MANAGER_H_
