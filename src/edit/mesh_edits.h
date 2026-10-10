#ifndef SNAPPER_EDIT_MESH_EDITS_H_
#define SNAPPER_EDIT_MESH_EDITS_H_

// Making a piece's warp again (a shape mesh at a new detail, a mesh
// for an old grid, a mesh for redrawn art) without losing what was set
// on its dots: RigManager and DollLibraryManager share these.

#include <QImage>
#include <QString>
#include <QStringList>

#include <vector>

#include "anim/mesh_carry.h"
#include "base/error.h"
#include "model/project.h"

namespace snapper {

// Every drawing of piece, read from its doll's folder; a drawing that
// does not read is left out.
std::vector<QImage> DrawingsOf(const Doll& doll, const QString& piece);

// The shape mesh of piece's drawings at detail, or why there is none.
Result<WarpGrid> ShapeMeshOf(const Doll& doll, const QString& piece,
                             MeshDetail detail);

// project with piece of doll warped by fresh instead: its dots'
// settings carried to the nearest new dots (CarryDots) and every key's
// warp on every layer showing doll carried by position (CarryOffsets).
// count adds up what carried and what was dropped.
Result<Project> WithWarp(const Project& project, const QString& doll,
                         const QString& piece, const WarpGrid& fresh,
                         CarryCount* count);

// project with the shape mesh of every mesh piece of doll made again
// from its drawings, at the detail it had; a piece whose mesh comes out
// the same (its drawings did not change) is left as it is. remeshed
// gets the names of the pieces that changed.
Project WithFreshMeshes(const Project& project, const QString& doll,
                        CarryCount* count, QStringList* remeshed);

}  // namespace snapper

#endif  // SNAPPER_EDIT_MESH_EDITS_H_
