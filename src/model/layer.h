#ifndef SNAPPER_MODEL_LAYER_H_
#define SNAPPER_MODEL_LAYER_H_

#include <QColor>
#include <QString>

#include <map>
#include <variant>

#include "base/frame.h"
#include "base/id.h"
#include "model/character.h"
#include "model/key.h"
#include "model/pose.h"
#include "model/storyboard.h"

namespace snapper {

struct LayerTag;
using LayerId = Id<LayerTag>;

// A posed character. One key holds its whole pose at that frame, as a
// drawing does in traditional animation; its place on the stage is the
// layer's own transform.
struct CharacterLayer final {
  QString character{};
  Channel<CharacterPose> poses{};
  // Drawn mirrored left to right.
  bool is_flipped = false;

  bool operator==(const CharacterLayer&) const = default;
};

// A single picture: a prop or a background.
struct ImageLayer final {
  QString path;

  bool operator==(const ImageLayer&) const = default;
};

// Lyrics and titles.
struct TextLayer final {
  QString text;
  QString font_family = QStringLiteral("Sans Serif");
  double size = 96.0;
  bool is_bold = true;
  QColor fill = Qt::white;
  QColor outline = Qt::black;
  double outline_width = 6.0;

  bool operator==(const TextLayer&) const = default;
};

enum class EffectKind {
  kFlash,      // Lightens toward color.
  kFill,       // Covers with color.
  kInvert,
  kZoomPunch,  // Scales everything below up by amount.
  kHalftone,
  kGlitch,     // RGB split and torn slices.
  kPosterize,
};
constexpr int kEffectKindCount = 7;

// Changes everything drawn below it, like an adjustment layer.
struct EffectLayer final {
  EffectKind kind = EffectKind::kFlash;
  QColor color = Qt::white;
  // 0 to 1, how strong. Keyed, so a flash can fade in two frames.
  Channel<double> amount;

  bool operator==(const EffectLayer&) const = default;
};

using LayerContent = std::variant<CharacterLayer, ImageLayer, TextLayer,
                                  EffectLayer, StoryboardLayer>;

// Something on a shot's stage. Every kind shares a name, a time range
// and a keyed transform; content says what it is.
struct Layer final {
  LayerId id;
  QString name;
  // Shown from start for length frames of the shot; length 0 means to
  // the end of the shot.
  Frame start;
  Frame length;
  bool is_visible = true;
  Channel<LayerPose> transform;
  LayerContent content;

  bool operator==(const Layer&) const = default;
};

// True on frames the layer shows, given the shot's length.
bool IsLayerLive(const Layer& layer, Frame frame, Frame shot_length);

// True for a layer that draws a picture in the camera's world, which a
// click on the stage can pick and a Comp effect can work on: a doll, a
// picture or text. An effect changes what lies below it, and a
// storyboard layer is a sheet laid over the canvas, so neither is one.
bool IsStagePicture(const Layer& layer);

}  // namespace snapper

#endif  // SNAPPER_MODEL_LAYER_H_
