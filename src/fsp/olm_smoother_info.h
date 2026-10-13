#ifndef SNAPPER_FSP_OLM_SMOOTHER_INFO_H_
#define SNAPPER_FSP_OLM_SMOOTHER_INFO_H_

#include "fsp/fsp_effects.h"

namespace snapper {

// The parameters of the Comp tab's Line smoother, a rebuild of OLM
// Smoother v2 (OLM Digital's After Effects plugin) from its manual. They
// are declared like a plugin's (FspEffectInfo), so the Comp effect keys,
// shows and saves them with the same machinery as F's Plugins; the
// smoother itself is not a plugin and is not in AllFspEffects.

// Value slots, in order. Choices count from 1 as After Effects does.
enum class SmootherSlot {
  kSmoothness,   // 0 to 100.
  kExtraSmooth,  // 0 to 100.
  kSmoothRange,  // 0 to 255: colors this close are one color.
  kUseColorKey,  // 0 or 1.
  kColorKey,     // 0xRRGGBB keyed to clear before smoothing.
  kVersion,      // 1 v1 (no sRGB to linear), 2 v2.
  kGamma,        // 1 Off, 2 All Colors, 3 Gamma Colors.
  kGammaValue,   // The gamma, 1 for none.
  kGammaCount,   // How many Gamma Colors are used, 1 to 4.
  kGammaColor0,  // The first of kSmootherGammaColors colors.
};
inline constexpr int kSmootherGammaColors = 4;
inline constexpr int kSmootherSlotCount =
    static_cast<int>(SmootherSlot::kGammaColor0) + kSmootherGammaColors;

// Choice numbers of kVersion and kGamma.
inline constexpr int kSmootherV1 = 1;
inline constexpr int kSmootherGammaOff = 1;
inline constexpr int kSmootherGammaAll = 2;
inline constexpr int kSmootherGammaListed = 3;

// The table, built once.
const FspEffectInfo& OlmSmootherInfo();

// The slot of a Gamma Color.
int SmootherGammaColorSlot(int index);

}  // namespace snapper

#endif  // SNAPPER_FSP_OLM_SMOOTHER_INFO_H_
