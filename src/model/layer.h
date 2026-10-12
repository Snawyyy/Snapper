#ifndef SNAPPER_MODEL_LAYER_H_
#define SNAPPER_MODEL_LAYER_H_

#include <QColor>
#include <QString>

#include <map>
#include <variant>

#include "base/frame.h"
#include "base/id.h"
#include "model/key.h"
#include "model/pose.h"

namespace snapper {

// A doll's pieces plus room for keys left by pieces renamed in Krita.
constexpr int kMaxPieceTracks = 256;

struct LayerTag;
using LayerId = Id<LayerTag>;

// How an animated object (a GIF) plays its frames on the shot clock.
// - kLoop: over and over.
// - kOnce: once, then stays on the last frame.
// - kPingPong: forward, then back, over and over.
// - kHold: does not play; shows the keyed drawing, for picking a frame
//   by hand.
enum class PlayMode { kLoop, kOnce, kPingPong, kHold };
inline constexpr int kPlayModeCount = 4;
// The most shot frames one drawing of an object may show for.
inline constexpr int kMaxFramesPerDrawing = 24;

// An object layer's play settings (see ObjectClip in model/doll.h).
struct ObjectPlay final {
  PlayMode mode = PlayMode::kLoop;
  // Shot frames each drawing shows for (2 is on twos); 0 keeps the
  // picture's own timing.
  int frames_per_drawing = 0;
  // The shot frame playing starts on; before it the first frame shows.
  Frame start;
  // How many frames in it starts, so two copies of one flame do not
  // flicker in step.
  int offset = 0;

  bool operator==(const ObjectPlay&) const = default;
};

// A posed doll. Each piece has its own channel of pose keys, by name.
struct DollLayer final {
  QString doll;
  std::map<QString, Channel<PiecePose>> pieces;
  // Drawn mirrored left to right.
  bool is_flipped = false;
  // Each smart dial's value (0 to 1) by dial name; a dial without keys
  // sits at its rest stop.
  std::map<QString, Channel<double>> dials{};
  // The skins it wears (track kind kSkin); without keys it wears none.
  Channel<SkinSet> skins{};
  // How an animated object plays; a doll that is not an object ignores
  // it.
  ObjectPlay play{};

  bool operator==(const DollLayer&) const = default;
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

using LayerContent =
    std::variant<DollLayer, ImageLayer, TextLayer, EffectLayer>;

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
  Channel<PiecePose> transform;
  LayerContent content;

  bool operator==(const Layer&) const = default;
};

// True on frames the layer shows, given the shot's length.
bool IsLayerLive(const Layer& layer, Frame frame, Frame shot_length);

}  // namespace snapper

#endif  // SNAPPER_MODEL_LAYER_H_
