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
  // K held: what this effect touches, white over black.
  CompEffectId mask_of{};

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
  QImage drawn;
};

// What the comp of shot at local needs from the layer pass: the target
// pictures by key, and whether drawn is needed.
struct CompNeeds final {
  std::set<CompTargetKey> targets;
  bool needs_drawn = false;

  bool IsEmpty() const { return targets.empty() && !needs_drawn; }
};
CompNeeds NeedsOf(const Shot& shot, Frame local, const CompShow& show);

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
};

// The space an effect's points live in, to output pixels: a layer
// target's own space (so they move with it), else the canvas without
// the camera (canvas pixels from the middle).
QTransform CompSpace(const CompContext& context, const CompEffect& effect);

// Runs shot's comp stack over frame, bottom effect first. costs, when
// given, gets each run effect's time in milliseconds.
void ApplyComp(const CompContext& context, QImage* frame,
               std::map<CompEffectId, double>* costs);

}  // namespace snapper

#endif  // SNAPPER_RENDER_COMP_H_
