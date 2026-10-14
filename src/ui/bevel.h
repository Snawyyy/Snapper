#ifndef SNAPPER_UI_BEVEL_H_
#define SNAPPER_UI_BEVEL_H_

#include <QRectF>

class QPainter;

namespace snapper {

// The old Windows edge round box, width pixels thick: light on the top
// and left with shadow on the bottom and right when raised, the other
// way round when sunk. Widgets that paint themselves use it, so their
// edges match the style's buttons and frames.
void DrawBevel(QPainter* painter, const QRectF& box, bool is_sunk,
               int width = 1);

}  // namespace snapper

#endif  // SNAPPER_UI_BEVEL_H_
