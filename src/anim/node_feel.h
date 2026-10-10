#ifndef SNAPPER_ANIM_NODE_FEEL_H_
#define SNAPPER_ANIM_NODE_FEEL_H_

#include <QString>

#include <vector>

#include "model/doll.h"

namespace snapper {

// How a moving dot feels, as two numbers instead of its five settings:
// firm (0 soft and loose, 1 firm and quick back) and heavy (0 light and
// quick to settle, 1 heavy, swinging through and wobbling long). The
// feel pad is these two; a preset is a named spot on it.
struct Feel final {
  double firm = 0.5;
  double heavy = 0.5;

  bool operator==(const Feel&) const = default;
};

// node with its settings set from feel, by its kind: Drag sets lag and
// bounce; Cloth and Jelly set stretch (squish), bounce, inertia, air
// resistance and gravity. Its point and kind stay.
DragNode WithFeel(DragNode node, const Feel& feel);

// The spot on the pad node's settings are nearest: exact for a node set
// by WithFeel, near for one fine-tuned after.
Feel FeelOf(const DragNode& node);

// A named feel for one kind ("Pudding" for jelly).
struct FeelPreset final {
  QString name;
  Feel feel;
};

// The presets of kind, in the order they are shown.
std::vector<FeelPreset> FeelPresets(NodeKind kind);

}  // namespace snapper

#endif  // SNAPPER_ANIM_NODE_FEEL_H_
