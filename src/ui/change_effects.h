#ifndef SNAPPER_UI_CHANGE_EFFECTS_H_
#define SNAPPER_UI_CHANGE_EFFECTS_H_

#include <QObject>

#include <optional>
#include <vector>

#include "base/frame.h"
#include "edit/change_diff.h"

namespace snapper {

class MotionManager;

// What a thing that just changed does on a timeline.
enum class ChangeEffectKind {
  // A new thing grows in from 1.4 times its size, ringed.
  kPop,
  // A deleted thing shrinks away and fades.
  kPuff,
  // A thing undo or redo brought back or moved flashes bright and
  // settles from 1.25 times its size.
  kRewind,
  // A key moved without a drag (spacing, undo or redo of a move) slides
  // from its old frame to the new one.
  kSlide
};

// One thing animating. lane is the row it sits on (a layer's id on the
// pose timeline, a track on the reel); item tells things on a lane apart
// (a clip's id; 0 for keys); t runs 0 to 1 as the effect plays, past 1
// for a moment when the curve overshoots.
struct ChangeEffect final {
  ChangeEffectKind kind = ChangeEffectKind::kPop;
  int lane = 0;
  int item = 0;
  Frame frame;
  Frame length = Frame(1);
  // Where a slide began; frame is where it ends.
  Frame from;
  double t = 0.0;
  int batch = 0;
};

// Size of a popping or rewinding thing at t, as a multiple of its own.
double PopScale(double t);
double RewindScale(double t);
// Size and opacity (1 solid, 0 gone) of a puffing thing at t.
double PuffScale(double t);
double PuffOpacity(double t);
// How bright a rewinding thing is at t, 1 flash down to 0 normal.
double RewindGlow(double t);

// Decorates timelines with what an edit did: new keys and clips pop,
// deleted ones puff out, and undo or redo (a rewind) flashes what it
// brings back. It only holds what to draw and when; the timeline draws
// it on top of the real project, so nothing here ever delays or alters
// data. Every effect runs through MotionManager, and in Off there are
// none, so the final state shows at once.
class ChangeEffects final : public QObject {
  Q_OBJECT

 public:
  ChangeEffects(MotionManager* motion, QObject* parent);

  // Starts the effects for an edit's key changes. is_travel is true for
  // undo and redo.
  void ShowKeys(const KeyChanges& changes, bool is_travel);
  void ShowClips(const ClipChanges& changes, bool is_travel);

  bool IsPlaying() const { return !effects_.empty(); }
  // Frames to add to a key's place to draw it where it is sliding from;
  // 0 when it is not sliding.
  double SlideOffsetOf(int lane, Frame frame) const;
  // The size to draw a thing still in the project at: 1 when nothing
  // pops or rewinds it.
  double ScaleOf(int lane, int item, Frame frame) const;
  // 0 to 1: how bright to draw a rewinding thing; 0 when it is not.
  double GlowOf(int lane, int item, Frame frame) const;
  // Progress of the ring around a popping thing, 0 to 1; nothing when
  // it has none.
  std::optional<double> RingOf(int lane, int item, Frame frame) const;
  // Things gone from the project that are still puffing out.
  std::vector<ChangeEffect> Puffs() const;

 signals:
  // Something to draw changed: repaint.
  void Changed();

 private:
  const ChangeEffect* Find(int lane, int item, Frame frame) const;
  // Plays effects together as one batch, timed by the pop time (or the
  // move time when no overshoot is wanted: puffs and slides).
  void Start(std::vector<ChangeEffect> effects, bool is_overshoot);
  void SetProgress(int batch, double t);
  void Finish(int batch);

  MotionManager* motion_;
  std::vector<ChangeEffect> effects_;
  int next_batch_ = 1;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHANGE_EFFECTS_H_
