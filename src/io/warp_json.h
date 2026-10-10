#ifndef SNAPPER_IO_WARP_JSON_H_
#define SNAPPER_IO_WARP_JSON_H_

#include <QJsonObject>

#include "io/json_values.h"
#include "model/doll.h"

namespace snapper {

// Writes a rig piece's warp into piece (one rig.json piece): the old
// grid as "warp" [columns, rows], and a shape mesh as "warp" [0, 0]
// (so an older Snapper reads it as no warp) with the mesh under "mesh":
// the drawing size it was made over, its detail, its dots as x, y
// pairs and its triangles as corner triples.
void WarpToJson(const WarpGrid& grid, QJsonObject* piece);

// The warp of piece. A broken mesh is noted and read as no warp; a
// grid is kept to 0 to kMaxWarpCells cells each way.
WarpGrid WarpFromJson(const QJsonObject& piece, JsonIssues* issues);

}  // namespace snapper

#endif  // SNAPPER_IO_WARP_JSON_H_
