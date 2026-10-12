#ifndef SNAPPER_ANIM_SKIN_H_
#define SNAPPER_ANIM_SKIN_H_

#include <QString>

#include <map>
#include <set>

#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"
#include "model/pose.h"

namespace snapper {

// The skins layer wears at frame (none without skin keys), keeping only
// those its doll still has.
SkinSet SkinsAt(const Doll& doll, const DollLayer& layer, Frame frame);

// The pieces whose hand pose in poses picks a drawing: a hand swap beats
// every skin.
std::set<QString> HandPicked(const std::map<QString, PiecePose>& poses);

// Puts the skins in worn on poses (the hand pose with the dials under
// it). For each piece not in hand_picked: a drawing a dial chose is
// swapped for the worn skin's version of it ("pajamas/mouth O" for
// "mouth O") where the doll has one; otherwise the first skin in the
// doll's list that names the piece sets its drawing, or hides it
// (opacity 0) when it names none. A drawing the piece no longer has
// leaves the piece as it was.
void ApplySkins(const Doll& doll, const SkinSet& worn,
                const std::set<QString>& hand_picked,
                std::map<QString, PiecePose>* poses);

// A skins channel that wears worn from the first frame: for showing a
// doll at rest in a skin.
Channel<SkinSet> WornFromStart(const SkinSet& worn);

// The prefixes ("pajamas" of "pajamas/arm") of every drawing of doll
// named "skin/drawing", in the order they are first met.
std::vector<QString> SkinPrefixes(const Doll& doll);

}  // namespace snapper

#endif  // SNAPPER_ANIM_SKIN_H_
