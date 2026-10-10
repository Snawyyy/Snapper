#ifndef SNAPPER_UI_DOT_KINDS_H_
#define SNAPPER_UI_DOT_KINDS_H_

#include <QString>

#include <array>
#include <vector>

#include "anim/node_feel.h"
#include "edit/rig_manager.h"
#include "model/doll.h"

namespace snapper {

// What a warp dot is made of. A dot with no drag node is Still.
enum class Material { kStill = 0, kDrag, kCloth, kJelly };
inline constexpr int kMaterialCount = 4;

// The material of a dot given its node (null for a plain dot).
Material MaterialOf(const DragNode* node);
// The node kind a material makes; Still has none and gives kDrag.
NodeKind KindOf(Material material);
// The words for a material: "Still", "Drag", "Cloth", "Jelly".
QString MaterialName(Material material);

// How much of a material the picked dots use.
enum class Share { kNone, kSome, kAll };

// The materials of the picked dots, counted. Decides which tile is
// chosen, which are partly used, and whether the pick is Mixed.
class KindMix final {
 public:
  // per_dot has one entry per picked dot: its node, or null if Still.
  explicit KindMix(const std::vector<const DragNode*>& per_dot);

  int total() const { return total_; }
  int CountOf(Material material) const;
  Share ShareOf(Material material) const;
  // More than one material among the picked dots.
  bool IsMixed() const;
  // The one material in use; Still when nothing is picked or mixed.
  Material Chosen() const;
  bool Uses(Material material) const { return CountOf(material) > 0; }

 private:
  std::array<int, kMaterialCount> counts_{};
  int total_ = 0;
};

// Whether a row's number is used by dots of material.
bool RowAppliesTo(DragField field, Material material);
// The number a row shows for node.
double FieldValue(const DragNode& node, DragField field);

// The exact rows that apply to the picked dots, in the order shown:
// every row any picked kind uses.
std::vector<DragField> FineRowsOf(const KindMix& mix);
// What a row is called: the stretch row is "Squish" when only jelly
// uses it, "Stretch" otherwise.
QString FineLabel(DragField field, const KindMix& mix);
// What a row's tooltip says.
QString FineHelp(DragField field, const KindMix& mix);

// The panel's title: "Warp dots: Chest, 6 picked", with ", Mixed" when
// the dots are of different materials. pieces are the picked dots'
// piece names, one per dot.
QString TitleOf(const std::vector<QString>& pieces, const KindMix& mix);

// The distinct feels (to 2 decimals) among nodes, at most kMaxPucks.
inline constexpr size_t kMaxPucks = 6;
std::vector<Feel> DistinctFeels(const std::vector<const DragNode*>& nodes);
// Whether two feels are the same spot on the pad.
bool SameFeel(const Feel& a, const Feel& b);

}  // namespace snapper

#endif  // SNAPPER_UI_DOT_KINDS_H_
