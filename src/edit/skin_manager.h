#ifndef SNAPPER_EDIT_SKIN_MANAGER_H_
#define SNAPPER_EDIT_SKIN_MANAGER_H_

#include <QString>

#include <set>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/project.h"
#include "model/skin.h"

namespace snapper {

class HistoryManager;

// What SetDrawing puts in a skin for a piece, besides a drawing index:
// leave the skin (the piece shows what it would without it) or hide it.
inline constexpr int kSkinUnset = -1;
inline constexpr int kSkinHidden = -2;

// Skins: making them on a doll (Rig tab), and wearing them on doll
// layers (Pose tab). A skin lives in the doll's rig, so it comes with
// the doll into every shot; which skins a layer wears is keyed per
// layer on its own track (kSkin), holding until the next key. Every
// call is one undo step.
class SkinManager final {
 public:
  explicit SkinManager(HistoryManager* history);

  // A new skin on doll named "Skin", "Skin 2"..., with the choices of
  // the skin called from (none when from is empty), at the end of the
  // list. Returns its name.
  Result<QString> AddSkin(const QString& doll, const QString& from);
  // Renames it, on every layer that wears it too.
  Result<void> RenameSkin(const QString& doll, const QString& skin,
                          const QString& to);
  // Refuses while a shot keys it (see WhyNoRemove).
  Result<void> RemoveSkin(const QString& doll, const QString& skin);
  // Moves it up (-1) or down (+1) the list; where two worn skins touch
  // the same piece, the one higher up wins.
  Result<void> MoveSkin(const QString& doll, const QString& skin, int by);
  // Sets what each of pieces shows in skin: a drawing index of the piece,
  // kSkinUnset or kSkinHidden.
  Result<void> SetDrawing(const QString& doll, const QString& skin,
                          const std::vector<QString>& pieces, int drawing);
  // Makes a skin for each "skin/drawing" prefix in doll's Krita layer
  // names that is not a skin yet: each piece with such a drawing shows
  // the skin's version of its default drawing, or its first one.
  // Returns how many were made.
  Result<int> MakeFromKritaNames(const QString& doll);

  // Puts skin on layers at frame, or takes it off when focus wears it
  // already. Without is_stacking it replaces what they wear; with it,
  // it is added. Layers whose doll has no such skin are left alone.
  // also lists more frames to do the same on (what focus wears at frame
  // decides putting on or taking off), all as one undo step.
  Result<void> Wear(ShotId shot, const std::vector<LayerId>& layers,
                    LayerId focus, Frame frame, const QString& skin,
                    bool is_stacking, const std::set<Frame>& also = {});
  // Wears the previous (-1) or next (+1) skin of focus's doll alone on
  // layers at frame; past either end wears none.
  Result<void> Step(ShotId shot, const std::vector<LayerId>& layers,
                    LayerId focus, Frame frame, int by);
  // Takes what layer shows at frame into skin: every piece whose hand
  // key there picks a drawing (or hides it with opacity 0) goes into
  // the skin, that hand pick goes back to the default, and the layer
  // wears the skin from frame, so the doll looks the same after.
  Result<void> UpdateFromLook(ShotId shot, LayerId layer, Frame frame,
                              const QString& skin);

  // The skins of doll, in list order.
  std::vector<Skin> SkinsOf(const QString& doll) const;
  // The doll a layer shows, or empty.
  QString DollOf(ShotId shot, LayerId layer) const;
  // The skins layer wears at frame.
  std::vector<QString> WornAt(ShotId shot, LayerId layer, Frame frame) const;
  // What piece shows in skin: a drawing index, kSkinUnset or
  // kSkinHidden.
  int DrawingIn(const QString& doll, const QString& skin,
                const QString& piece) const;
  // Why skin can't be deleted (the shots that key it); empty when it can.
  QString WhyNoRemove(const QString& doll, const QString& skin) const;
  // Why MakeFromKritaNames would make none; empty when it would.
  QString WhyNoKritaSkins(const QString& doll) const;

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SKIN_MANAGER_H_
