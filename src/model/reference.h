#ifndef SNAPPER_MODEL_REFERENCE_H_
#define SNAPPER_MODEL_REFERENCE_H_

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>

#include <optional>
#include <vector>

#include "base/frame.h"
#include "base/id.h"

namespace snapper {

struct ReferenceTag;
using ReferenceId = Id<ReferenceTag>;

constexpr int kMaxReferencesPerShot = 8;
constexpr int kMaxReferenceMarks = 256;
constexpr double kMinReferenceSpeed = 0.1;
constexpr double kMaxReferenceSpeed = 4.0;
constexpr double kDefaultReferenceOpacity = 0.5;
// How small or big a reference may be placed, in canvas pixels per
// picture pixel.
constexpr double kMinReferenceScale = 0.01;
constexpr double kMaxReferenceScale = 100.0;

// A light-table tint over a reference, so it never reads as the art.
enum class ReferenceTint { kNone, kBlue, kRed };
constexpr int kReferenceTintCount = 3;

// A video, GIF or picture shown on a shot's Pose tab stage only, to
// animate over. It is linked by path, never copied, and never drawn in
// the shot's own pictures (export, the Video tab, thumbnails): only the
// stage draws it. It is not a layer and has no keys.
struct Reference final {
  ReferenceId id;
  // Linked, not copied: a moved file keeps these settings until found.
  QString path;
  // The picture's size in pixels and the video's length in seconds (0
  // for a still picture), read when it was added.
  QSize size;
  double seconds = 0.0;
  double opacity = kDefaultReferenceOpacity;
  // Over every layer (checking a silhouette) instead of behind them.
  bool is_in_front = false;
  // Its own eye; Y hides every reference at once on top of this.
  bool is_shown = true;
  bool is_flipped = false;
  ReferenceTint tint = ReferenceTint::kNone;
  // Placed in canvas pixels from the canvas middle, without the shot's
  // camera, so it stays put while the camera moves.
  QPointF center;
  // Canvas pixels per picture pixel.
  double scale = 1.0;
  // The shot frame its first shown moment plays on.
  Frame start;
  // Seconds into the file shown at start (the slide under the row).
  double skip = 0.0;
  // 1 plays at its own speed; 0.5 slows it to half.
  double speed = 1.0;
  // Moments marked on it (a foot down, a hit), in seconds of the file.
  std::vector<double> marks;

  bool IsStill() const { return seconds <= 0.0; }
  bool operator==(const Reference&) const = default;
};

const Reference* FindReference(const std::vector<Reference>& references,
                               ReferenceId id);
Reference* FindReference(std::vector<Reference>* references, ReferenceId id);
// One more than the highest id in references.
ReferenceId NextReferenceId(const std::vector<Reference>& references);

// The moment of the file shown at shot frame local: nothing before its
// start; past its end it is the end (IsReferenceEnded). A still picture
// shows 0 from its start on.
std::optional<double> ReferenceSecondsAt(const Reference& reference,
                                         Frame local);
// True when the video has run out by local: it holds its last frame.
bool IsReferenceEnded(const Reference& reference, Frame local);
// The shot frame (possibly fractional or before 0) a moment of the file
// plays on.
double ReferenceFrameOf(const Reference& reference, double seconds);
// The shot frame its video runs out on; the start for a still picture.
double ReferenceEndFrame(const Reference& reference);
// Where it is drawn, in canvas pixels (the canvas corner at 0,0).
QRectF ReferenceRect(const Reference& reference, QSize canvas);
// The scale that fits it inside the canvas, or to the canvas height.
double FitFrameScale(const Reference& reference, QSize canvas);
double FitHeightScale(const Reference& reference, QSize canvas);

}  // namespace snapper

#endif  // SNAPPER_MODEL_REFERENCE_H_
