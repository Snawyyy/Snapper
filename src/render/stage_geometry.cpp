#include "render/stage_geometry.h"

#include <cassert>
#include <variant>

#include "anim/links.h"
#include "render/frame_renderer.h"

namespace snapper {

std::optional<QTransform> LayerToScreen(const Project& project,
                                        const Shot& shot, LayerId layer,
                                        Frame local, double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  if (found == nullptr) {
    return std::nullopt;
  }
  LinkSolver links(project, shot);
  return links.LayerTransform(*found, local) *
         FrameRenderer::ViewTransform(project, shot, local, scale);
}

QTransform LayerParentToScreen(const Project& project, const Shot& shot,
                               const Layer& layer, Frame local, double scale) {
  assert(scale > 0.0);
  assert(layer.id.value() >= 0);
  return FrameRenderer::ViewTransform(project, shot, local, scale);
}

std::vector<PosedBone> CharacterBonesOnScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              Frame local, double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  const auto* cast =
      found != nullptr ? std::get_if<CharacterLayer>(&found->content) : nullptr;
  const Character* character =
      cast != nullptr ? FindCharacter(project, cast->character) : nullptr;
  const auto to_screen =
      character != nullptr ? LayerToScreen(project, shot, layer, local, scale)
                           : std::nullopt;
  if (!to_screen) {
    return {};
  }
  std::vector<PosedBone> bones =
      PoseBones(*character, PoseAt(*character, *cast, local));
  for (PosedBone& bone : bones) {
    bone.base = to_screen->map(bone.base);
    bone.tip = to_screen->map(bone.tip);
  }
  return bones;
}

}  // namespace snapper
