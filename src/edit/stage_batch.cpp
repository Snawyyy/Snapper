// StageManager's changes to many layers at once.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/stage_manager.h"

namespace snapper {
namespace {

Error NonePicked() {
  const QString text = Tr("Pick one or more layers first.");
  assert(!text.isEmpty());
  return Error{text};
}

// The project with change(Layer*) applied to each of ids in shot; ids
// that are gone are skipped, and none at all is refused.
template <typename Change>
Result<Project> WithLayers(const Project& project, ShotId shot,
                           const std::vector<LayerId>& ids, Change change) {
  assert(shot.value() >= 0);
  assert(ids.size() <= static_cast<size_t>(kMaxLayersPerShot));
  return WithShot(project, shot, [&](Shot* edited) -> Result<void> {
    int changed = 0;
    for (const LayerId id : ids) {
      Layer* layer = id.IsValid() ? FindLayer(edited, id) : nullptr;
      const bool is_found = layer != nullptr;
      if (is_found) {
        change(layer);
        ++changed;
      }
    }
    return changed > 0 ? Result<void>() : std::unexpected(NonePicked());
  });
}

}  // namespace

Result<std::vector<LayerId>> StageManager::DuplicateAll(
    ShotId shot, const std::vector<LayerId>& ids) {
  assert(history_ != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxLayersPerShot));
  std::vector<LayerId> copies;
  {
    EditScope batch(history_, Tr("Duplicate layers"));
    for (const LayerId id : ids) {
      const auto copy = Duplicate(shot, id);
      if (!copy) {
        batch.Cancel();
        return std::unexpected(copy.error());
      }
      copies.push_back(*copy);
    }
  }
  return copies;
}

Result<std::vector<LayerId>> StageManager::AddDolls(
    ShotId shot, const QStringList& dolls) {
  assert(history_ != nullptr);
  assert(dolls.size() <= kMaxLayersPerShot);
  std::vector<LayerId> added;
  EditScope batch(history_, Tr("Add dolls"));
  for (const QString& doll : dolls) {
    const auto layer = AddDoll(shot, doll);
    if (!layer) {
      batch.Cancel();
      return std::unexpected(layer.error());
    }
    added.push_back(*layer);
  }
  return added;
}

Result<void> StageManager::RemoveAll(ShotId shot,
                                     const std::vector<LayerId>& ids) {
  assert(history_ != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxLayersPerShot));
  return history_->Apply(
      Tr("Remove layers"),
      WithShot(history_->current(), shot, [&ids](Shot* edited) {
        const auto removed = std::erase_if(
            edited->layers, [&ids](const Layer& layer) {
              return std::find(ids.begin(), ids.end(), layer.id) != ids.end();
            });
        return removed > 0 ? Result<void>() : std::unexpected(NonePicked());
      }));
}

Result<void> StageManager::ShowAll(ShotId shot,
                                   const std::vector<LayerId>& ids,
                                   bool is_visible) {
  assert(history_ != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxLayersPerShot));
  return history_->Apply(
      is_visible ? Tr("Show layers") : Tr("Hide layers"),
      WithLayers(history_->current(), shot, ids, [is_visible](Layer* layer) {
        layer->is_visible = is_visible;
      }));
}

Result<void> StageManager::FlipAll(ShotId shot,
                                   const std::vector<LayerId>& ids,
                                   bool is_flipped) {
  assert(history_ != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxLayersPerShot));
  return history_->Apply(
      Tr("Flip dolls"),
      WithLayers(history_->current(), shot, ids, [is_flipped](Layer* layer) {
        auto* doll = std::get_if<DollLayer>(&layer->content);
        const bool is_doll = doll != nullptr;
        if (is_doll) {
          doll->is_flipped = is_flipped;
        }
      }));
}

Result<void> StageManager::Restack(ShotId shot,
                                   const std::vector<LayerId>& ids,
                                   int step) {
  assert(history_ != nullptr);
  assert(step == 1 || step == -1);
  const auto is_picked = [&ids](const Layer& layer) {
    return std::find(ids.begin(), ids.end(), layer.id) != ids.end();
  };
  return history_->Apply(
      step > 0 ? Tr("Raise layers") : Tr("Lower layers"),
      WithShot(history_->current(), shot, [&](Shot* edited) {
        auto& layers = edited->layers;
        const int count = static_cast<int>(layers.size());
        // Walk from the end they move towards, so neighbours that are
        // both picked keep their order.
        for (int k = 0; k < count; ++k) {
          const int i = step > 0 ? count - 1 - k : k;
          const int j = i + step;
          const bool can_swap = j >= 0 && j < count &&
                                is_picked(layers[static_cast<size_t>(i)]) &&
                                !is_picked(layers[static_cast<size_t>(j)]);
          if (can_swap) {
            std::swap(layers[static_cast<size_t>(i)],
                      layers[static_cast<size_t>(j)]);
          }
        }
        return Result<void>();
      }));
}

Result<void> StageManager::ShiftTiming(ShotId shot,
                                       const std::vector<LayerId>& ids,
                                       int start_delta, int length_delta) {
  assert(history_ != nullptr);
  assert(std::abs(start_delta) <= kMaxFrame);
  return history_->Apply(
      Tr("Change layer timing"),
      WithLayers(history_->current(), shot, ids, [=](Layer* layer) {
        layer->start = Frame(layer->start.index() + start_delta);
        layer->length = Frame(layer->length.index() + length_delta);
      }));
}

Result<void> StageManager::ShiftText(ShotId shot,
                                     const std::vector<LayerId>& ids,
                                     double size_delta,
                                     double outline_delta) {
  assert(history_ != nullptr);
  assert(std::isfinite(size_delta) && std::isfinite(outline_delta));
  return history_->Apply(
      Tr("Edit text"),
      WithLayers(history_->current(), shot, ids, [=](Layer* layer) {
        auto* text = std::get_if<TextLayer>(&layer->content);
        const bool is_text = text != nullptr;
        if (is_text) {
          text->size = std::max(1.0, text->size + size_delta);
          text->outline_width =
              std::max(0.0, text->outline_width + outline_delta);
        }
      }));
}

Result<void> StageManager::StyleText(ShotId shot,
                                     const std::vector<LayerId>& ids,
                                     bool is_bold, QColor fill,
                                     QColor outline) {
  assert(history_ != nullptr);
  const bool is_valid = fill.isValid() && outline.isValid();
  if (!is_valid) {
    return std::unexpected(Error{Tr("That isn't a colour.")});
  }
  return history_->Apply(
      Tr("Edit text"),
      WithLayers(history_->current(), shot, ids, [&](Layer* layer) {
        auto* text = std::get_if<TextLayer>(&layer->content);
        const bool is_text = text != nullptr;
        if (is_text) {
          text->is_bold = is_bold;
          text->fill = fill;
          text->outline = outline;
        }
      }));
}

Result<void> StageManager::SetEffectAll(ShotId shot,
                                        const std::vector<LayerId>& ids,
                                        EffectKind kind, QColor color) {
  assert(history_ != nullptr);
  const bool is_valid = color.isValid();
  if (!is_valid) {
    return std::unexpected(Error{Tr("That isn't a colour.")});
  }
  return history_->Apply(
      Tr("Change effect"),
      WithLayers(history_->current(), shot, ids, [&](Layer* layer) {
        auto* effect = std::get_if<EffectLayer>(&layer->content);
        const bool is_effect = effect != nullptr;
        if (is_effect) {
          effect->kind = kind;
          effect->color = color;
        }
      }));
}

}  // namespace snapper
