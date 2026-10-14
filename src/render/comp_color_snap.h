#ifndef SNAPPER_RENDER_COMP_COLOR_SNAP_H_
#define SNAPPER_RENDER_COMP_COLOR_SNAP_H_

#include <QImage>

#include "fsp/fsp_effects.h"

namespace snapper {

// Some plugins (SelectiveColorBlur) match their target colors exactly,
// so a pose frame between keys, whose warped art shifts colors by a few
// steps, loses most of the pixels the plugin should find. This is the
// host side of a color reach for them.

// True for the plugins that match target colors exactly.
bool MatchesColorsExactly(const FspEffectInfo& info);

// A copy of source (straight alpha, same format) where every pixel
// within reach (RGB distance) of an enabled target color of the plugin
// (its TurnOnN and targetColorN values) has that color exactly; the
// nearest wins. Alpha is kept, and see-through pixels are left alone,
// since their color means nothing. source itself when the plugin has no
// target colors on.
QImage SnapToTargetColors(const FspEffectInfo& info, const FspValues& values,
                          const QImage& source, double reach);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_COLOR_SNAP_H_
