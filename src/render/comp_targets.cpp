#include "render/comp_targets.h"

#include <algorithm>
#include <cassert>

namespace snapper {

bool HasCompLayer(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) >= 0);
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  return effect.target.HasLayers();
}

bool NeedsCompShape(const CompEffect& effect) {
  assert(static_cast<int>(effect.kind) >= 0);
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  return effect.kind == CompKind::kRimLight ||
         effect.kind == CompKind::kTransmittedLight ||
         (effect.kind == CompKind::kGradient &&
          effect.shape == GradientShape::kFromEdge);
}

CompTargetKey CompKeyOf(const CompEffect& effect) {
  assert(HasCompLayer(effect));
  assert(effect.target.parts.size() <= static_cast<size_t>(kMaxLayersPerShot));
  CompTargetKey key{effect.target.parts};
  for (CompTargetPart& part : key.parts) {
    std::ranges::sort(part.pieces);
  }
  std::ranges::sort(key.parts);
  return key;
}

const CompTargetPart* PartOf(const CompTargetKey& key, LayerId layer) {
  assert(layer.IsValid());
  assert(key.parts.size() <= static_cast<size_t>(kMaxLayersPerShot));
  for (const CompTargetPart& part : key.parts) {
    const bool is_layer = part.layer == layer;
    if (is_layer) {
      return &part;
    }
  }
  return nullptr;
}

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
    const bool has_layer = HasCompLayer(effect);
    if (has_layer) {
      needs.targets.insert(CompKeyOf(effect));
      continue;
    }
    needs.needs_drawn = needs.needs_drawn || NeedsCompShape(effect);
  }
  return needs;
}

QTransform CompSpace(const CompContext& context, const CompEffect& effect) {
  assert(context.project != nullptr && context.shot != nullptr);
  assert(context.scale > 0.0);
  // Several layers share no space, so only a lone one carries the points.
  const bool is_lone = effect.target.parts.size() == 1;
  const Layer* layer =
      is_lone ? FindLayer(*context.shot, effect.target.parts.front().layer)
              : nullptr;
  const bool is_layer_space = layer != nullptr && context.links != nullptr;
  if (is_layer_space) {
    return context.links->LayerTransform(*layer, context.local) *
           context.view;
  }
  const CanvasSize& canvas = context.project->canvas;
  return QTransform::fromTranslate(canvas.width / 2.0, canvas.height / 2.0) *
         QTransform::fromScale(context.scale, context.scale);
}

}  // namespace snapper
