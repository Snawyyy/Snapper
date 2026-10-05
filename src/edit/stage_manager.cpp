#include "edit/stage_manager.h"

#include <QFileInfo>
#include <QImageReader>

#include <cassert>
#include <variant>

#include "edit/history_manager.h"
#include "edit/project_edits.h"

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
                                       const QString& label) {
  assert(history_ != nullptr);
  assert(!name.isEmpty() && !label.isEmpty());
  Project next = history_->current();
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
    edited->layers.push_back(std::move(layer));
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
  const bool is_present = FindDoll(history_->current(), doll) != nullptr;
  if (!is_present) {
    return std::unexpected(
        Error{Tr("Import %1 into the project first.").arg(doll)});
  }
  return AddLayer(shot, doll, DollLayer{doll, {}, false},
                  Tr("Add %1").arg(doll));
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
