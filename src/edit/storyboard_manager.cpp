#include "edit/storyboard_manager.h"

#include <QDir>
#include <QFile>
#include <QPainter>

#include <cassert>

#include "base/text.h"
#include "edit/document_manager.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/sidecar_folder.h"
#include "edit/storyboard_edits.h"
#include "edit/storyboard_folder.h"
#include "render/frame_renderer.h"
#include "render/storyboard_paint.h"

namespace snapper {

StoryboardManager::StoryboardManager(HistoryManager* history,
                                     DocumentManager* document,
                                     DrawManager* draw,
                                     SelectionManager* selection,
                                     const QString& data_folder)
    : history_(history),
      document_(document),
      draw_(draw),
      selection_(selection),
      data_folder_(data_folder) {
  assert(history_ != nullptr && document_ != nullptr);
  assert(draw_ != nullptr && selection_ != nullptr);
}

LayerId StoryboardManager::Active(ShotId shot) const {
  assert(history_ != nullptr && selection_ != nullptr);
  assert(shot.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const bool has_shot = found != nullptr;
  if (!has_shot) {
    return LayerId();
  }
  const bool has_cel_pick = picked_shot_ == shot && picked_layer_.IsValid();
  const LayerId wanted = has_cel_pick ? picked_layer_ : selection_->layer();
  return ActiveStoryboard(*found, wanted);
}

LayerId StoryboardManager::PickedBoard(ShotId shot) const {
  assert(history_ != nullptr && selection_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_cels = !Picked(shot, picked_layer_).empty();
  if (has_cels) {
    return picked_layer_;
  }
  const bool is_board_picked = Of(shot, selection_->layer()) != nullptr;
  return is_board_picked ? selection_->layer() : LayerId();
}

const StoryboardLayer* StoryboardManager::Of(ShotId shot,
                                             LayerId layer) const {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const bool can_find = found != nullptr && layer.IsValid();
  const Layer* inside = can_find ? FindLayer(*found, layer) : nullptr;
  const bool has_layer = inside != nullptr;
  return has_layer ? BoardOf(*inside) : nullptr;
}

QString StoryboardManager::Folder() const {
  assert(document_ != nullptr);
  assert(!data_folder_.isEmpty() || document_->HasPath());
  const bool is_saved = document_->HasPath();
  return StoryboardFolder(is_saved ? document_->path() : QString(),
                          data_folder_);
}

QString StoryboardManager::WhyNoDraw(ShotId shot, Frame frame,
                                     bool is_over) const {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const bool is_busy = draw_->is_drawing();
  if (is_busy) {
    return Tr("Already drawing: press Done or Cancel first.");
  }
  const Shot* found = FindShot(history_->current(), shot);
  const bool has_shot = found != nullptr;
  if (!has_shot) {
    return Tr("Pick a shot first.");
  }
  const bool is_past_end = frame >= found->length;
  if (is_past_end) {
    return Tr("The playhead is past the shot's end.");
  }
  const StoryboardLayer* board = Of(shot, Active(shot));
  const bool has_board = board != nullptr;
  if (is_over) {
    const bool is_showing = has_board && CelAt(*board, frame) != nullptr;
    return is_showing ? QString()
                      : Tr("No storyboard frame shows here to draw over.");
  }
  const bool is_taken = has_board && CelStartingAt(*board, frame) != nullptr;
  if (is_taken) {
    return Tr("A frame starts here already: draw over it instead.");
  }
  const bool is_full =
      has_board &&
      board->cels.size() >= static_cast<size_t>(kMaxStoryboardCels);
  return is_full ? Tr("A storyboard holds at most %1 frames.")
                       .arg(kMaxStoryboardCels)
                 : QString();
}

QImage StoryboardManager::Blank() const {
  assert(history_ != nullptr);
  const CanvasSize canvas = history_->current().canvas;
  assert(IsValidCanvas(canvas));
  QImage image(QSize(canvas.width, canvas.height), QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  return image;
}

QImage StoryboardManager::CelImage(const QString& path) const {
  assert(!path.isEmpty());
  QImage image = Blank();
  const QImage read(path);
  const bool is_readable = !read.isNull();
  if (is_readable) {
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(QRectF(image.rect()), read);
  }
  assert(image.format() == QImage::Format_ARGB32);
  return image;
}

QImage StoryboardManager::Under(const Shot& shot, LayerId layer,
                                Frame frame) const {
  assert(frame.index() >= 0);
  assert(IsValidCanvas(history_->current().canvas));
  // The sheet being drawn must not lie under itself.
  Shot without = shot;
  Layer* hidden = layer.IsValid() ? FindLayer(&without, layer) : nullptr;
  const bool has_layer = hidden != nullptr;
  if (has_layer) {
    hidden->is_visible = false;
  }
  // A renderer of its own shares no cache and draws only the sheets
  // ticked for export.
  FrameRenderer renderer;
  return renderer.RenderShot(history_->current(), without, frame, 1.0);
}

Result<QString> StoryboardManager::Save(const QImage& image,
                                        const Shot& shot,
                                        Frame frame) const {
  assert(!image.isNull());
  assert(frame.index() >= 0);
  const QString folder = Folder();
  const bool is_made = QDir().mkpath(folder);
  if (!is_made) {
    return std::unexpected(Error{Tr("Can't make the folder %1.").arg(folder)});
  }
  const QString stem = QStringLiteral("%1 frame %2")
                           .arg(SafeFileName(shot.name))
                           .arg(frame.index());
  const QString path =
      FreshNumberedPath(QDir(folder), stem, QStringLiteral("png"));
  const bool is_saved = image.save(path, "PNG");
  if (!is_saved) {
    return std::unexpected(
        Error{Tr("Can't save the storyboard frame in %1.").arg(folder)});
  }
  return path;
}

Result<void> StoryboardManager::DrawNew(ShotId shot, Frame frame) {
  assert(frame.index() >= 0);
  assert(shot.value() >= 0);
  return Start(shot, frame, false);
}

Result<void> StoryboardManager::DrawOver(ShotId shot, Frame frame) {
  assert(frame.index() >= 0);
  assert(shot.value() >= 0);
  return Start(shot, frame, true);
}

Result<void> StoryboardManager::Start(ShotId shot, Frame frame,
                                      bool is_over) {
  assert(history_ != nullptr && draw_ != nullptr);
  assert(frame.index() >= 0);
  const QString why_not = WhyNoDraw(shot, frame, is_over);
  const bool is_refused = !why_not.isEmpty();
  if (is_refused) {
    return std::unexpected(Error{why_not});
  }
  const Shot& found = *FindShot(history_->current(), shot);
  const LayerId layer = Active(shot);
  const StoryboardLayer none;
  const StoryboardLayer* board = Of(shot, layer);
  const StoryboardLayer& cels = board != nullptr ? *board : none;
  const StoryboardCel* showing = CelAt(cels, frame);
  const Frame at = is_over ? showing->at : frame;
  // The onion: the frame before this one, to trace from.
  const StoryboardCel* before = CelBefore(cels, at);
  DrawJob job;
  job.target = DrawTarget::kStoryboard;
  job.title = (is_over ? Tr("Storyboard: %1, frame %2")
                       : Tr("Storyboard: %1, starting at frame %2"))
                  .arg(found.name)
                  .arg(at.index());
  job.image = is_over ? CelImage(showing->path) : Blank();
  job.reference = showing != nullptr ? CelImage(showing->path) : Blank();
  job.under = Under(found, layer, frame);
  const bool has_before = before != nullptr;
  if (has_before) {
    job.onions.push_back(TintedCel(CelImage(before->path), kOnionBefore));
  }
  job.keep = [this, shot, layer, at, is_over](const QImage& image) {
    return Keep(image, shot, layer, at, is_over);
  };
  return draw_->BeginJob(std::move(job));
}

Result<void> StoryboardManager::Keep(const QImage& image, ShotId shot,
                                     LayerId layer, Frame at, bool is_over) {
  assert(history_ != nullptr);
  assert(!image.isNull());
  const Shot* found = FindShot(history_->current(), shot);
  const bool has_shot = found != nullptr;
  if (!has_shot) {
    return std::unexpected(Error{Tr("That shot no longer exists.")});
  }
  const auto saved = Save(image, *found, at);
  if (!saved) {
    return std::unexpected(saved.error());
  }
  auto applied = history_->Apply(
      is_over ? Tr("Edit storyboard frame") : Tr("Draw storyboard frame"),
      WithCel(shot, layer, at, *saved, is_over));
  if (!applied) {
    QFile::remove(*saved);
  }
  return applied;
}

Result<Project> StoryboardManager::WithCel(ShotId shot, LayerId layer,
                                           Frame at, const QString& path,
                                           bool is_over) const {
  assert(history_ != nullptr);
  assert(!path.isEmpty());
  Project next = history_->current();
  const bool is_new_layer = !layer.IsValid();
  const LayerId id = is_new_layer ? TakeLayerId(&next) : layer;
  return WithShot(std::move(next), shot, [&](Shot* edited) -> Result<void> {
    if (is_new_layer) {
      const bool is_full =
          edited->layers.size() >= static_cast<size_t>(kMaxLayersPerShot);
      if (is_full) {
        return std::unexpected(Error{
            Tr("A shot holds at most %1 layers.").arg(kMaxLayersPerShot)});
      }
      Layer fresh;
      fresh.id = id;
      fresh.name = NextStoryboardName(*edited);
      fresh.content = StoryboardLayer{};
      edited->layers.push_back(std::move(fresh));
    }
    Layer* target = FindLayer(edited, id);
    StoryboardLayer* board = target != nullptr ? BoardOf(target) : nullptr;
    const bool has_board = board != nullptr;
    if (!has_board) {
      return std::unexpected(Error{Tr("That storyboard is gone.")});
    }
    const bool is_missing = is_over && CelStartingAt(*board, at) == nullptr;
    if (is_missing) {
      return std::unexpected(Error{Tr("That storyboard frame is gone.")});
    }
    // A new frame shows, so Done shows what was drawn.
    target->is_visible = true;
    return PutCel(board, at, path);
  });
}

}  // namespace snapper
