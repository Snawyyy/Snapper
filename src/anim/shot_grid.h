#ifndef SNAPPER_ANIM_SHOT_GRID_H_
#define SNAPPER_ANIM_SHOT_GRID_H_

#include <QPointF>
#include <QSize>
#include <QString>
#include <QTransform>

#include <map>
#include <optional>
#include <vector>

#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// Every point of a piece's warp grid as it is carried at a frame, in
// shot space, and the drawing-to-shot map it went through.
struct ShotGrid final {
  std::vector<QPointF> points;
  QTransform to_shot;
};

// How far links carry piece at frame, in shot space.
QPointF ShiftOf(const LinkShifts& shifts, const QString& piece, int frame);

// Where piece's grid stands at frame under poses and the layer's move,
// slid by shift (what links carry it). Nothing when the piece is not on
// the doll. The points are empty when the piece has no warp grid.
std::optional<ShotGrid> ShotGridOf(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const QString& piece,
    Frame frame, QPointF shift);

// Which way a drawing's down points in shot space when to_shot carries
// it there, a unit step (straight down when to_shot squashes it flat).
QPointF DrawnDown(const QTransform& to_shot);

// How long a cell of grid over a drawing of size is in shot space when
// to_shot carries the drawing there, across and down on average.
double CellLength(const WarpGrid& grid, QSize size, const QTransform& to_shot);

}  // namespace snapper

#endif  // SNAPPER_ANIM_SHOT_GRID_H_
