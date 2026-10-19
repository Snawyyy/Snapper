#include "edit/stage_manager.h"

#include <QFileInfo>
#include <QImageReader>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/doll_convert.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/storyboard_edits.h"

namespace snapper {

QString EffectName(EffectKind kind) {
  assert(static_cast<int>(kind) < kEffectKindCount);
  assert(kEffectKindCount == 7);
  switch (kind) {
    case EffectKind::kFlash:
      return Tr("Flash");
    case EffectKind::kFill:
      return Tr("Fill");
    case EffectKind::kInvert:
      return Tr("Invert");
    case EffectKind::kZoomPunch:
      return Tr("Zoom punch");
    case EffectKind::kHalftone:
      return Tr("Halftone");
    case EffectKind::kGlitch:
      return Tr("Glitch");
    case EffectKind::kPosterize:
      return Tr("Posterize");
  }
  return QString();
}

StageManager::StageManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<LayerId> StageManager::AddLayer(ShotId shot, const QString& name,
                                       LayerContent content,
                                       const QString& label, bool is_bottom,
                                       std::optional<PiecePose> pose,
                                       std::optional<Project> base) {
  assert(history_ != nullptr);
  assert(!name.isEmpty() && !label.isEmpty());
  Project next = base.has_value() ? std::move(*base) : history_->current();
  const LayerId id = TakeLayerId(&next);
  auto added = WithShot(std::move(next), shot, [&](Shot* edited) {
    const bool is_full =
        edited->layers.size() >= static_cast<size_t>(kMaxLayersPerShot);
    if (is_full) {
      return Result<void>(std::unexpected(Error{
          Tr("A shot holds at most %1 layers.").arg(kMaxLayersPerShot)}));
    }
    Layer layer;
    layer.id = id;
    layer.name = name;
    layer.content = std::move(content);
    if (pose) {
      layer.transform.keys.push_back(Key<PiecePose>{Frame(0), *pose});
    }
    if (is_bottom) {
      edited->layers.insert(edited->layers.begin(), std::move(layer));
    } else {
      edited->layers.push_back(std::move(layer));
    }
    return Result<void>();
  });
  auto applied = history_->Apply(label, std::move(added));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<LayerId> StageManager::AddDoll(ShotId shot, const QString& doll) {
  assert(history_ != nullptr);
  assert(!doll.isEmpty());
  if (FindCharacter(history_->current(), doll) != nullptr) {
    return AddCharacter(shot, doll);
  }
  const Doll* found = FindDoll(history_->current(), doll);
  if (found == nullptr) {
    return std::unexpected(
        Error{Tr("Import %1 into the project first.").arg(doll)});
  }
  // A doll comes onto the stage as a character; a prop picture stays a
  // doll so its GIF keeps playing.
  if (!found->rig.object.is_object) {
    Project next = history_->current();
    DollParts parts;
    next.characters[doll] =
        std::make_shared<const Character>(CharacterFromDoll(*found, &parts));
    return AddLayer(shot, doll, CharacterLayer{doll, {}, false},
                    Tr("Add %1").arg(doll), false, std::nullopt,
                    std::move(next));
  }
  return AddLayer(shot, doll, DollLayer{doll, {}, false},
                  Tr("Add %1").arg(doll));
}

Result<LayerId> StageManager::AddCharacter(ShotId shot,
                                           const QString& character) {
  assert(history_ != nullptr);
  assert(!character.isEmpty());
  if (FindCharacter(history_->current(), character) == nullptr) {
    return std::unexpected(
        Error{Tr("Add %1 to the project first.").arg(character)});
  }
  return AddLayer(shot, character, CharacterLayer{character, {}, false},
                  Tr("Add %1").arg(character));
}

Result<LayerId> StageManager::AddImage(ShotId shot, const QString& path) {
  assert(history_ != nullptr);
  assert(!path.isEmpty());
  QImageReader reader(path);
  const bool is_image = reader.canRead();
  if (!is_image) {
    return std::unexpected(
        Error{Tr("%1 isn't a picture Snapper can read.").arg(path)});
  }
  const QFileInfo info(path);
  return AddLayer(shot, info.completeBaseName(),
                  ImageLayer{info.absoluteFilePath()}, Tr("Add picture"));
}

Result<LayerId> StageManager::AddBackground(ShotId shot,
                                            const QString& path) {
  assert(history_ != nullptr);
  assert(!path.isEmpty());
  QImageReader reader(path);
  const QSize size = reader.canRead() ? reader.size() : QSize();
  const bool is_image = !size.isEmpty();
  if (!is_image) {
    return std::unexpected(
        Error{Tr("%1 isn't a picture Snapper can read.").arg(path)});
  }
  // The picture sits centred on the canvas; the larger of the two
  // scales covers it edge to edge.
  const CanvasSize& canvas = history_->current().canvas;
  const double cover =
      std::max(static_cast<double>(canvas.width) / size.width(),
               static_cast<double>(canvas.height) / size.height());
  PiecePose pose;
  pose.scale_x = cover;
  pose.scale_y = cover;
  const QFileInfo info(path);
  return AddLayer(shot, Tr("Background"),
                  ImageLayer{info.absoluteFilePath()}, Tr("Add background"),
                  true, pose);
}

Result<LayerId> StageManager::AddText(ShotId shot, const QString& text) {
  assert(history_ != nullptr);
  assert(text.size() < 100000);
  const bool is_blank = text.trimmed().isEmpty();
  if (is_blank) {
    return std::unexpected(Error{Tr("Type some text first.")});
  }
  TextLayer layer;
  layer.text = text;
  return AddLayer(shot, text.simplified().left(24), layer, Tr("Add text"));
}

Result<LayerId> StageManager::AddEffect(ShotId shot, EffectKind kind) {
  assert(history_ != nullptr);
  assert(static_cast<int>(kind) < kEffectKindCount);
  EffectLayer effect;
  effect.kind = kind;
  return AddLayer(shot, EffectName(kind), effect,
                  Tr("Add %1").arg(EffectName(kind)));
}

Result<LayerId> StageManager::AddStoryboard(ShotId shot) {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  const bool has_shot = found != nullptr;
  if (!has_shot) {
    return std::unexpected(Error{Tr("Pick a shot first.")});
  }
  assert(found->layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  return AddLayer(shot, NextStoryboardName(*found), StoryboardLayer{},
                  Tr("Add storyboard"));
}

Result<LayerId> StageManager::Duplicate(ShotId shot, LayerId layer) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  Project next = history_->current();
  const LayerId id = TakeLayerId(&next);
  auto copied = WithShot(std::move(next), shot, [&](Shot* edited) {
    const Layer* found = FindLayer(*edited, layer);
    const bool can_copy =
        found != nullptr &&
        edited->layers.size() < static_cast<size_t>(kMaxLayersPerShot);
    if (!can_copy) {
      return Result<void>(
          std::unexpected(Error{Tr("That layer can't be copied.")}));
    }
    Layer copy = *found;
    copy.id = id;
    copy.name = Tr("%1 copy").arg(copy.name);
    const auto at = edited->layers.begin() + (found - edited->layers.data());
    edited->layers.insert(at + 1, std::move(copy));
    return Result<void>();
  });
  if (copied) {
    *copied = WithCopiedLayerDots(std::move(*copied), {{layer, id}});
  }
  auto applied = history_->Apply(Tr("Duplicate layer"), std::move(copied));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<void> StageManager::Remove(ShotId shot, LayerId layer) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  return history_->Apply(
      Tr("Remove layer"),
      WithShot(history_->current(), shot, [layer](Shot* edited) {
        const auto removed = std::erase_if(
            edited->layers,
            [layer](const Layer& item) { return item.id == layer; });
        const bool is_removed = removed > 0;
        return is_removed ? Result<void>()
                          : Result<void>(std::unexpected(Error{
                                Tr("That layer no longer exists.")}));
      }));
}

Result<void> StageManager::Move(ShotId shot, LayerId layer, int index) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  return history_->Apply(
      Tr("Restack layer"),
      WithShot(history_->current(), shot, [layer, index](Shot* edited) {
        const Layer* found = FindLayer(*edited, layer);
        const int count = static_cast<int>(edited->layers.size());
        const bool is_valid = found != nullptr && index >= 0 && index < count;
        if (!is_valid) {
          return Result<void>(
              std::unexpected(Error{Tr("A layer can't go there.")}));
        }
        const auto from = found - edited->layers.data();
        Layer moved = std::move(edited->layers[static_cast<size_t>(from)]);
        edited->layers.erase(edited->layers.begin() + from);
        edited->layers.insert(edited->layers.begin() + index,
                              std::move(moved));
        return Result<void>();
      }));
}

}  // namespace snapper
