#ifndef SNAPPER_RENDER_COMP_H_
#define SNAPPER_RENDER_COMP_H_

#include <QImage>
#include <QTransform>

#include <compare>
#include <map>
#include <set>
#include <vector>

#include "anim/links.h"
#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// What the Comp tab's stage asks of the comp; export never sets it.
struct CompShow final {
  // V held: every effect off.
  bool is_off = false;
  // K held: what this effect touches, white over black; mask_shot is
  // its shot (ids repeat between shots), any shot when invalid.
  CompEffectId mask_of{};
  ShotId mask_shot{};

  bool operator==(const CompShow&) const = default;
};

// A target layer, or some pieces of it, whose own picture an effect
// needs.
struct CompTargetKey final {
  LayerId layer;
  std::vector<QString> pieces;

  auto operator<=>(const CompTargetKey&) const = default;
};

// The pictures the comp works on, made while the layers are drawn: each
// target as it shows in the frame (what lies above it cut away), and,
// when asked, every layer's pixels on clear (the shape of "everything").
struct CompPictures final {
  std::map<CompTargetKey, QImage> targets;
  // What stands in front of each target (the doll's own pieces above
  // the picked ones, then later layers), cut out of what its effects
  // draw so the target keeps its place in the draw order. None when
  // nothing does.
  std::map<CompTargetKey, QImage> covers;
  QImage drawn;
};

// What the comp of shot at local needs from the layer pass: the target
// pictures by key, and whether drawn is needed.
struct CompNeeds final {
  std::set<CompTargetKey> targets;
  bool needs_drawn = false;

  bool IsEmpty() const { return targets.empty() && !needs_drawn; }
};
// counts_screen false leaves the screen effects' needs out.
CompNeeds NeedsOf(const Shot& shot, Frame local, const CompShow& show,
                  bool counts_screen = true);

struct CompContext final {
  const Project* project = nullptr;
  const Shot* shot = nullptr;
  Frame local;
  double scale = 1.0;
  // Shot space to output pixels, with the camera.
  QTransform view;
  LinkSolver* links = nullptr;
  const CompPictures* pictures = nullptr;
  CompShow show;
  // On the reel the screen effects run over the whole reel picture
  // (render/comp_reel.cpp), so the shot's own pass skips them.
  bool skips_screen = false;
  // The reel's pass runs just this screen effect, already known to be
  // running; nullptr runs the shot's stack.
  const CompEffect* only = nullptr;
};

// The space an effect's points live in, to output pixels: a layer
// target's own space (so they move with it), else the canvas without
// the camera (canvas pixels from the middle).
QTransform CompSpace(const CompContext& context, const CompEffect& effect);

// What running the comp found, per effect: its time and whether a
// plugin effect (kFsp) refused to run and was skipped.
struct CompReport final {
  // Milliseconds, per effect run.
  std::map<CompEffectId, double> costs;
  // Plugin effects whose last run gave no picture.
  std::set<CompEffectId> failed;
};

// Runs shot's comp stack over frame, bottom effect first. report, when
// given, gets each run effect's time and outcome.
void ApplyComp(const CompContext& context, QImage* frame,
               CompReport* report);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_H_
