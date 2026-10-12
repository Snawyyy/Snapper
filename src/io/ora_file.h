#ifndef SNAPPER_IO_ORA_FILE_H_
#define SNAPPER_IO_ORA_FILE_H_

#include <QString>

#include "base/error.h"
#include "model/doll.h"

namespace snapper {

// The hidden layer of joint crosses on top of a doll turned back into a
// Krita file. The Krita exporter skips a layer of this name.
inline constexpr char kRigLayerName[] = "Snapper rig";

// Writes doll as an OpenRaster file Krita opens with its groups: each
// piece with several drawings a group of layers named as in Krita (the
// default one shown), a piece with one drawing a plain layer, the
// folder groups where they were, bottom to top as the doll draws. The
// PNGs are copied from doll.folder byte for byte. The canvas is the
// doll's Krita canvas, grown evenly on each side when a piece reaches
// past it, so the middle (doll space's origin) stays put and a
// re-export lands in the same place.
Result<void> WriteDollOra(const Doll& doll, const QString& path);

}  // namespace snapper

#endif  // SNAPPER_IO_ORA_FILE_H_
