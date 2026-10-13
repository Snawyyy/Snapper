#include "render/comp.h"

#include <QElapsedTimer>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <optional>

#include "anim/comp_fsp.h"
#include "anim/comp_knobs.h"
#include "render/blend.h"
#include "render/comp_effects.h"
#include "render/comp_masks.h"
#include "render/comp_plugin.h"

namespace snapper {
namespace {

// The color key's reach for picked target colors (RGB distance).
constexpr double kTargetTolerance = 24.0;
constexpr double kTargetFeather = 0.5;

bool HasLayer(const CompEffect& effect) {
  return effect.target.layer.IsValid();
}

// Kinds that read the target's outline.
bool NeedsShape(const CompEffect& effect) {
  return effect.kind == CompKind::kRimLight ||
         effect.kind == CompKind::kTransmittedLight ||
         (effect.kind == CompKind::kGradient &&
          effect.shape == GradientShape::kFromEdge);
}

CompTargetKey KeyOf(const CompEffect& effect) {
  assert(HasLayer(effect));
  std::vector<QString> pieces = effect.target.pieces;
  std::ranges::sort(pieces);
  return CompTargetKey{effect.target.layer, pieces};
}

// The seed of a moving effect: new every rate frames, held at 0.
int SeedOf(const CompEffect& effect, Frame local, double rate) {
  const int every = static_cast<int>(std::lround(rate));
  const int step = every > 0 ? local.index() / every : 0;
  return step * 131 + effect.id.value() * 7919;
}

// One effect's picture and the mask it is laid down through.
struct Drawn final {
  QImage picture;
  Mask mask;
};

class CompRun final {
 public:
  CompRun(const CompContext& context, QImage* frame, CompReport* report)
      : context_(context), frame_(frame), report_(report) {
    assert(frame_ != nullptr && context_.shot != nullptr);
    assert(context_.pictures != nullptr);
  }

  // false when the effect touches nothing this frame (its layer is not
  // drawn now, or its plugin is not in this Snapper): it then costs
  // nothing.
  bool Run(const CompEffect& effect) {
    assert(effect.is_on || context_.show.mask_of == effect.id);
    const Frame local = context_.local;
    const bool is_placeholder =
        effect.kind == CompKind::kFsp && FspInfoOf(effect) == nullptr;
    if (is_placeholder) {
      return false;
    }
    const auto shape = ShapeOf(effect);
    const bool is_drawn = shape.has_value();
    if (!is_drawn) {
      return false;
    }
    const Mask region = RegionOf(effect, *shape);
    const bool is_mask_view = context_.show.mask_of == effect.id;
    if (is_mask_view) {
      ShowMask(region, frame_);
      return true;
    }
    const Drawn drawn = Draw(effect, *shape, region);
    const bool is_skipped = drawn.picture.isNull();
    NoteOutcome(effect.id, is_skipped);
    if (is_skipped) {
      return true;
    }
    BlendOnto(drawn.picture, effect.blend,
              KnobAt(effect, CompKnob::kAmount, local), drawn.mask, frame_);
    return true;
  }

  // Colors are keyed on the frame before any effect, so a swap below
  // never moves another effect's pick.
  void KeepBase() { base_ = frame_->copy(); }

 private:
  // Keeps whether the effect's last run gave a picture.
  void NoteOutcome(CompEffectId id, bool is_failed) {
    assert(id.IsValid());
    const bool is_reported = report_ != nullptr;
    if (!is_reported) {
      return;
    }
    assert(report_->failed.size() <=
           static_cast<size_t>(kMaxCompEffectsPerShot) * 2);
    if (is_failed) {
      report_->failed.insert(id);
    } else {
      report_->failed.erase(id);
    }
  }

  // The target's own shape: empty for the whole frame; nothing when the
  // target layer is not drawn now.
  std::optional<Mask> ShapeOf(const CompEffect& effect) const {
    const bool has_layer = HasLayer(effect);
    if (has_layer) {
      const auto found = context_.pictures->targets.find(KeyOf(effect));
      const bool is_drawn = found != context_.pictures->targets.end() &&
                            !found->second.isNull();
      if (!is_drawn) {
        return std::nullopt;
      }
      return AlphaOf(found->second);
    }
    const bool wants_drawn =
        NeedsShape(effect) && !context_.pictures->drawn.isNull();
    return wants_drawn ? AlphaOf(context_.pictures->drawn) : Mask();
  }

  Mask RegionOf(const CompEffect& effect, const Mask& shape) const {
    const bool has_colors = !effect.target.colors.empty() &&
                            effect.kind != CompKind::kLineSmoother;
    if (!has_colors) {
      return shape;
    }
    const QImage& keyed = base_.isNull() ? *frame_ : base_;
    const Mask colors = ColorMaskOf(keyed, effect.target.colors,
                                    kTargetTolerance, kTargetFeather);
    return shape.empty() ? colors : Both(shape, colors);
  }

  const QImage& TargetPicture(const CompEffect& effect) const {
    const bool has_layer = HasLayer(effect);
    if (has_layer) {
      return context_.pictures->targets.at(KeyOf(effect));
    }
    return *frame_;
  }

  Drawn Draw(const CompEffect& effect, const Mask& shape,
             const Mask& region) const {
    const Frame local = context_.local;
    const double scale = context_.scale;
    const int width = frame_->width();
    const int height = frame_->height();
    const auto knob = [&](CompKnob which) {
      return KnobAt(effect, which, local);
    };
    const bool is_whole_shape = shape.empty();
    const bool needs_full = (effect.kind == CompKind::kTransmittedLight &&
                             region.empty()) ||
                            (effect.kind == CompKind::kRimLight &&
                             is_whole_shape);
    const Mask full = needs_full ? Mask(static_cast<size_t>(width) *
                                            static_cast<size_t>(height),
                                        255)
                                 : Mask();
    switch (effect.kind) {
      case CompKind::kLineSmoother:
        return {SmoothLines(*frame_, effect.target.colors), shape};
      case CompKind::kColorSwap:
        return {SwapColors(*frame_, effect.swaps, knob(CompKnob::kSize),
                           knob(CompKnob::kSoftness)),
                region};
      case CompKind::kTransmittedLight: {
        const bool is_inside = effect.style == 1;
        const Mask& glow_shape = region.empty() ? full : region;
        QImage glow = EdgeGlow(glow_shape, width, height, effect.color,
                               std::max(1.0, knob(CompKnob::kSize) * scale),
                               knob(CompKnob::kSoftness), is_inside);
        return {glow, Mask()};
      }
      case CompKind::kRimLight: {
        const Mask& edge = is_whole_shape ? full : shape;
        QImage band = RimBand(edge, width, height, effect.color,
                              LightAt(*context_.shot, effect, local),
                              std::max(1.0, knob(CompKnob::kSize) * scale),
                              knob(CompKnob::kSoftness));
        const bool has_colors = !effect.target.colors.empty();
        return {band, has_colors ? region : Mask()};
      }
      case CompKind::kGradient:
        return DrawGradient(effect, shape, region);
      case CompKind::kMotionBlur: {
        const QImage source = Masked(TargetPicture(effect),
                                     HasLayer(effect) ? Mask() : region);
        return {SmearAlong(source, knob(CompKnob::kAngle),
                           knob(CompKnob::kSize) * scale,
                           knob(CompKnob::kSoftness),
                           SeedOf(effect, local, 1.0)),
                Mask()};
      }
      case CompKind::kSparkle:
        return {Sparkles(Masked(*frame_, region), effect.color,
                         knob(CompKnob::kSoftness),
                         static_cast<int>(std::lround(knob(CompKnob::kCount))),
                         knob(CompKnob::kSize) * scale,
                         knob(CompKnob::kAngle), scale),
                Mask()};
      case CompKind::kSpeedLines: {
        const QPointF center =
            CompSpace(context_, effect).map(PointAAt(effect, local));
        return {SpeedLines(frame_->size(), effect.style, center,
                           knob(CompKnob::kAngle),
                           static_cast<int>(std::lround(knob(CompKnob::kCount))),
                           std::max(0.5, knob(CompKnob::kSize) * scale),
                           knob(CompKnob::kSoftness), effect.color,
                           SeedOf(effect, local, knob(CompKnob::kRate))),
                region};
      }
      case CompKind::kFinish:
        return {Finish(*frame_, effect.style,
                       std::max(1.0, knob(CompKnob::kSize) * scale),
                       static_cast<int>(std::lround(knob(CompKnob::kCount))),
                       SeedOf(effect, local, knob(CompKnob::kRate))),
                region};
      case CompKind::kFsp:
        return DrawPlugin(effect, region);
    }
    return {QImage(frame_->size(), frame_->format()), Mask()};
  }

  // The plugin works on the target's own pixels, like motion blur: a
  // layer's picture as it shows, else the frame cut to the region. On a
  // layer what it draws past the layer (an outline) stays; on the frame
  // picked colors keep it to those pixels. A null picture skips it.
  Drawn DrawPlugin(const CompEffect& effect, const Mask& region) const {
    assert(effect.kind == CompKind::kFsp);
    assert(context_.scale > 0.0);
    const bool has_layer = HasLayer(effect);
    const QImage source =
        Masked(TargetPicture(effect), has_layer ? Mask() : region);
    return {RunCompPlugin(effect, source, context_.local, context_.scale),
            has_layer ? Mask() : region};
  }

  Drawn DrawGradient(const CompEffect& effect, const Mask& shape,
                     const Mask& region) const {
    const Frame local = context_.local;
    GradientPlace place;
    place.shape = effect.shape;
    place.a = PointAAt(effect, local);
    place.b = PointBAt(effect, local);
    place.space = CompSpace(context_, effect);
    place.steps = static_cast<int>(
        std::lround(KnobAt(effect, CompKnob::kCount, local)));
    // Shot light: the ramp keeps its middle and length and turns to
    // run from the light.
    const bool is_lit = effect.uses_shot_light &&
                        (effect.shape == GradientShape::kLinear ||
                         effect.shape == GradientShape::kRadial);
    if (is_lit) {
      const double radians =
          ShotLightAt(*context_.shot, local) * std::numbers::pi / 180.0;
      const QPointF middle = (place.a + place.b) / 2.0;
      const QPointF axis = place.b - place.a;
      const double half = std::hypot(axis.x(), axis.y()) / 2.0;
      const QPointF toward(std::sin(radians), -std::cos(radians));
      place.a = middle + toward * half;
      place.b = middle - toward * half;
    }
    const bool is_edge_whole =
        shape.empty() && effect.shape == GradientShape::kFromEdge;
    const Mask full = is_edge_whole
                          ? Mask(static_cast<size_t>(frame_->width()) *
                                     static_cast<size_t>(frame_->height()),
                                 255)
                          : Mask();
    place.edge = shape.empty() ? &full : &shape;
    place.edge_size = std::max(
        1.0, KnobAt(effect, CompKnob::kSize, local) * context_.scale);
    const bool has_stops = !effect.stops.empty();
    const std::vector<GradientStop> stops =
        has_stops ? effect.stops
                  : std::vector<GradientStop>{{0.0, Qt::black},
                                              {1.0, Qt::white}};
    return {GradientOver(stops, place, frame_->width(), frame_->height()),
            region};
  }

  const CompContext& context_;
  QImage* frame_;
  // Not owned; nullptr when nobody asks.
  CompReport* report_;
  QImage base_;
};

}  // namespace

CompNeeds NeedsOf(const Shot& shot, Frame local, const CompShow& show) {
  assert(local.index() >= 0);
  assert(shot.comp.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  CompNeeds needs;
  const bool is_off = show.is_off && !show.mask_of.IsValid();
  if (is_off) {
    return needs;
  }
  for (const CompEffect& effect : shot.comp) {
    const bool is_run = effect.is_on || show.mask_of == effect.id;
    if (!is_run) {
      continue;
    }
    if (HasLayer(effect)) {
      needs.targets.insert(KeyOf(effect));
    } else if (NeedsShape(effect)) {
      needs.needs_drawn = true;
    }
  }
  return needs;
}

QTransform CompSpace(const CompContext& context, const CompEffect& effect) {
  assert(context.project != nullptr && context.shot != nullptr);
  assert(context.scale > 0.0);
  const bool has_layer = HasLayer(effect);
  const Layer* layer =
      has_layer ? FindLayer(*context.shot, effect.target.layer) : nullptr;
  const bool is_layer_space = layer != nullptr && context.links != nullptr;
  if (is_layer_space) {
    return context.links->LayerTransform(*layer, context.local) *
           context.view;
  }
  const CanvasSize& canvas = context.project->canvas;
  return QTransform::fromTranslate(canvas.width / 2.0, canvas.height / 2.0) *
         QTransform::fromScale(context.scale, context.scale);
}

void ApplyComp(const CompContext& context, QImage* frame,
               CompReport* report) {
  assert(frame != nullptr && !frame->isNull());
  assert(context.shot != nullptr && context.pictures != nullptr);
  const Shot& shot = *context.shot;
  const bool is_mask_view = context.show.mask_of.IsValid();
  const bool is_off = context.show.is_off && !is_mask_view;
  const bool is_idle = is_off || shot.comp.empty();
  if (is_idle) {
    return;
  }
  CompRun run(context, frame, report);
  const bool keys_colors = std::ranges::any_of(
      shot.comp, [](const CompEffect& effect) {
        return effect.is_on && !effect.target.colors.empty();
      });
  if (keys_colors) {
    run.KeepBase();
  }
  for (const CompEffect& effect : shot.comp) {
    const bool is_shown_mask = context.show.mask_of == effect.id;
    const bool is_run = (effect.is_on && !context.show.is_off) || is_shown_mask;
    if (!is_run) {
      continue;
    }
    const bool is_weak =
        KnobAt(effect, CompKnob::kAmount, context.local) <= 0.0;
    const bool is_skipped = is_weak && !is_shown_mask;
    if (is_skipped) {
      continue;
    }
    QElapsedTimer clock;
    clock.start();
    run.Run(effect);
    const bool is_reported = report != nullptr;
    if (is_reported) {
      report->costs[effect.id] =
          static_cast<double>(clock.nsecsElapsed()) / 1e6;
    }
    if (is_shown_mask) {
      return;
    }
  }
}

}  // namespace snapper
