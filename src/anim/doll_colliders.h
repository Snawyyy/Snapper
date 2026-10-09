#ifndef SNAPPER_ANIM_DOLL_COLLIDERS_H_
#define SNAPPER_ANIM_DOLL_COLLIDERS_H_

#include <QString>
#include <QTransform>

#include <map>
#include <vector>

#include "anim/collider.h"
#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// The edges that push the cloth of piece at frame, in shot space: the
// colliders of the doll's other pieces drawn behind piece (a lower
// draw order), except those that spare it, each carried by its piece's
// pose, warp, the layer's move and what links carry it.
std::vector<ColliderEdge> CollidersFor(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const LinkShifts& shifts,
    const QString& piece, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_COLLIDERS_H_
