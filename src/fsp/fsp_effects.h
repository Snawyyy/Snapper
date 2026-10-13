#ifndef SNAPPER_FSP_FSP_EFFECTS_H_
#define SNAPPER_FSP_FSP_EFFECTS_H_

#include <QImage>
#include <QString>
#include <QStringList>

#include <vector>

namespace snapper {

// F's Plugins (bryful, MIT licence) running inside Snapper. The real
// After Effects plugin code lives in third_party/fsp and runs against a
// small stand-in for the After Effects host; this header is all the rest
// of Snapper sees of it. See docs/features/fsp-effects.md.

enum class FspParamType {
  kSlider,    // A whole number.
  kFloat,     // A number with decimals.
  kAngle,     // Degrees.
  kCheckbox,  // 0 or 1.
  kPopup,     // The choice's number, from 1 as After Effects counts.
  kColor,     // 0xRRGGBB.
  kPoint,     // Two slots: x then y, in pixels of the picture.
};

// One parameter, as the plugin declares it (read by running the plugin's
// own parameter setup, never typed in by hand).
struct FspParam {
  QString name;
  FspParamType type = FspParamType::kFloat;
  double low = 0.0;      // Smallest value that may be typed.
  double high = 1.0;     // Largest value that may be typed.
  double neutral = 0.0;  // The plugin's own default.
  QStringList choices;   // Popup entries, in order.
  // Where the value starts in the plugin's values; a point takes two.
  int slot = 0;
  // A parameter of the plugin's own that has no meaning here (a layer
  // picker, a button) is kept but not shown.
  bool is_hidden = false;
};

struct FspEffectInfo {
  // The plugin's name as a folder: "RimFill". Saved in project files.
  QString id;
  // Its name as After Effects showed it.
  QString name;
  // The plugin's own parameters in order; slot counts follow them.
  std::vector<FspParam> params;
  int slot_count = 0;
};

// One value per slot (see FspParam::slot).
using FspValues = std::vector<double>;

// Every plugin Snapper carries, by name. The parameters come from running
// each plugin's setup once, the first time this is called.
const std::vector<FspEffectInfo>& AllFspEffects();
// nullptr when id names none.
const FspEffectInfo* FindFspEffect(const QString& id);
// The plugin's own defaults, one per slot.
FspValues DefaultFspValues(const FspEffectInfo& info);

// Runs the plugin over input (any size; read as straight alpha) and gives
// back a picture of the same size. scale is how much smaller than full
// size the picture is (a preview at half size is 0.5), so pixel widths
// and distances in values shrink with it as they do in After Effects'
// half-resolution preview. Gives back a null image when the plugin
// refused or the values do not fit it.
QImage RunFspEffect(const FspEffectInfo& info, const FspValues& values,
                    const QImage& input, double scale = 1.0);

}  // namespace snapper

#endif  // SNAPPER_FSP_FSP_EFFECTS_H_
