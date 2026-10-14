#ifndef SNAPPER_RENDER_COMP_PLUGIN_H_
#define SNAPPER_RENDER_COMP_PLUGIN_H_

#include <QImage>

#include "base/frame.h"
#include "model/comp.h"

namespace snapper {

// Runs a plugin effect's (kFsp) plugin over source with its values at
// local, for a frame drawn at scale (a half size render is 0.5), and
// gives the picture back in source's format. Null when the plugin is
// missing, refused, or gave a picture of another size: the effect is
// then skipped for this frame.
QImage RunCompPlugin(const CompEffect& effect, const QImage& source,
                     Frame local, double scale);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_PLUGIN_H_
