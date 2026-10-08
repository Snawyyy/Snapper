#ifndef SNAPPER_IO_POINT_MOTION_JSON_H_
#define SNAPPER_IO_POINT_MOTION_JSON_H_

#include <QJsonArray>
#include <QJsonObject>

#include <vector>

#include "io/json_values.h"
#include "model/doll.h"

namespace snapper {

// A rig piece's point motions, as rig.json keeps them under "motions".
QJsonArray PointMotionsToJson(const std::vector<PointMotion>& motions);

// The point motions of piece (one rig.json piece) on grid: once per
// point, every number kept in range, and none on points off the grid.
std::vector<PointMotion> PointMotionsFromJson(const QJsonObject& piece,
                                              WarpGrid grid,
                                              JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_POINT_MOTION_JSON_H_
