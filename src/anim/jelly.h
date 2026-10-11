#ifndef SNAPPER_ANIM_JELLY_H_
#define SNAPPER_ANIM_JELLY_H_

#include <QPointF>

#include <vector>

#include "anim/cloth.h"
#include "model/doll.h"

namespace snapper {

// Runs one frame of the jelly of piece: its nodes wobble as one soft
// blob. Each node rides the body and keeps its own speed (so it lags,
// overshoots and settles), is pulled back toward where the body carries
// it, and then takes a share of the way to the average swing of the
// nodes beside it, so neighbours move together and the blob squashes
// and stretches smoothly with no spikes. The settings are its
// JellyMotion: the squish (how little neighbours are held together),
// the pull, the speed kept and the fall. Colliders work as for cloth.
// Deterministic: the same input gives the same output.
void StepJelly(const std::vector<QPointF>& carried, ClothPiece* piece);

}  // namespace snapper

#endif  // SNAPPER_ANIM_JELLY_H_
