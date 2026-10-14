#include "edit/storyboard_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "base/text.h"
#include "edit/document_manager.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "render/storyboard_paint.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

// The most tries for a free file name.
constexpr int kMaxNameTries = 100000;

// The cel of draft starting at frame, for editing, or nullptr.
StoryboardCel* CelStartingAt(Draft* draft, Frame frame) {
  assert(draft != nullptr);
  assert(frame.index() >= 0);
  const auto found =
      std::ranges::find_if(draft->cels, [frame](const StoryboardCel& cel) {
        return cel.at == frame;
      });
  return found == draft->cels.end() ? nullptr : &*found;
}

QString CelLabel(const char* one, const char* many, size_t count) {
  assert(one != nullptr && many != nullptr);
  assert(count > 0);
  return count == 1 ? Tr(one) : Tr(many).arg(count);
}

}  // namespace

StoryboardManager::StoryboardManager(HistoryManager* history, DocumentManager* document,
                           DrawManager* draw)
    : history_(history), document_(document), draw_(draw) {
  assert(history_ != nullptr);
  assert(document_ != nullptr && draw_ != nullptr);
}

const Draft* StoryboardManager::Of(ShotId shot) const {
  assert(history_ != nullptr);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr ? &found->draft : nullptr;
}

QString StoryboardManager::Folder() const {
  assert(document_ != nullptr);
  if (document_->HasPath()) {
    const QFileInfo file(document_->path());
    return QDir(file.absolutePath())
        .filePath(file.completeBaseName() + QStringLiteral(" drafts"));
  }
  const QFileInfo settings(document_->SettingsPath());
  return QDir(settings.absolutePath()).filePath(QStringLiteral("drafts"));
}

QString StoryboardManager::WhyNoDraw(ShotId shot, Frame frame,
                                bool is_over) const {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  if (draw_->is_drawing()) {
    return Tr("Already drawing: press Done or Cancel first.");
  }
  const Shot* found = FindShot(history_->current(), shot);
  if (found == nullptr) {
    return Tr("Pick a shot first.");
  }
  if (frame >= found->length) {
    return Tr("The playhead is past the shot's end.");
  }
  const Draft& draft = found->draft;
  if (is_over) {
    return CelAt(draft, frame) == nullptr
               ? Tr("No draft cel shows here to draw over.")
               : QString();
  }
  if (snapper::CelStartingAt(draft, frame) != nullptr) {
    return Tr("A cel starts here already: draw over it instead.");
  }
  const bool is_full =
      draft.cels.size() >= static_cast<size_t>(kMaxStoryboardCels);
  return is_full ? Tr("A draft holds at most %1 cels.").arg(kMaxStoryboardCels)
                 : QString();
}

QImage StoryboardManager::Blank() const {
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
  if (!read.isNull()) {
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(QRectF(image.rect()), read);
  }
  assert(image.format() == QImage::Format_ARGB32);
  return image;
}

QImage StoryboardManager::Under(const Shot& shot, Frame frame) const {
  assert(frame.index() >= 0);
  assert(IsValidCanvas(history_->current().canvas));
  // A renderer of its own keeps the draft off and shares no cache.
  FrameRenderer renderer;
  return renderer.RenderShot(history_->current(), shot, frame, 1.0);
}

Result<QString> StoryboardManager::Save(const QImage& image, ShotId shot,
                                   Frame frame) const {
  assert(!image.isNull());
  assert(frame.index() >= 0);
  const QString folder = Folder();
  if (!QDir().mkpath(folder)) {
    return std::unexpected(Error{Tr("Can't make the folder %1.").arg(folder)});
  }
  for (int number = 1; number <= kMaxNameTries; ++number) {
    const QString path = QDir(folder).filePath(
        QStringLiteral("shot %1 frame %2 v%3.png")
            .arg(shot.value())
            .arg(frame.index())
            .arg(number));
    if (QFile::exists(path)) {
      continue;
    }
    if (!image.save(path, "PNG")) {
      break;
    }
    return path;
  }
  return std::unexpected(Error{Tr("Can't save the draft cel in %1.").arg(folder)});
}

Result<void> StoryboardManager::DrawNew(ShotId shot, Frame frame) {
  return Start(shot, frame, false);
}

Result<void> StoryboardManager::DrawOver(ShotId shot, Frame frame) {
  return Start(shot, frame, true);
}

Result<void> StoryboardManager::Start(ShotId shot, Frame frame, bool is_over) {
  assert(history_ != nullptr && draw_ != nullptr);
  assert(frame.index() >= 0);
  const QString why_not = WhyNoDraw(shot, frame, is_over);
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  const Shot& found = *FindShot(history_->current(), shot);
  const Draft& draft = found.draft;
  const StoryboardCel* showing = CelAt(draft, frame);
  const Frame at = is_over ? showing->at : frame;
  // The onion: the last cel before this one and the first after it.
  const StoryboardCel* before = nullptr;
  for (const StoryboardCel& cel : draft.cels) {
    if (cel.at < at) {
      before = &cel;
    }
  }
  const StoryboardCel* after = CelAfter(draft, at);
  DrawJob job;
  job.target = DrawTarget::kStoryboard;
  job.title = is_over ? Tr("Draft: %1, cel at frame %2")
                            .arg(found.name)
                            .arg(at.index())
                      : Tr("Draft: %1, new cel at frame %2")
                            .arg(found.name)
                            .arg(at.index());
  job.image = is_over ? CelImage(showing->path) : Blank();
  job.reference = showing != nullptr ? CelImage(showing->path) : Blank();
  job.under = Under(found, frame);
  if (before != nullptr) {
    job.onions.push_back(TintedCel(CelImage(before->path), kOnionBefore));
  }
  if (after != nullptr) {
    job.onions.push_back(TintedCel(CelImage(after->path), kOnionAfter));
  }
  job.keep = [this, shot, at, is_over](const QImage& image) -> Result<void> {
    const auto saved = Save(image, shot, at);
    if (!saved) {
      return std::unexpected(saved.error());
    }
    const QString path = *saved;
    auto applied = history_->Apply(
        is_over ? Tr("Edit draft cel") : Tr("Draw draft cel"),
        WithShot(history_->current(), shot,
                 [&](Shot* edited) -> Result<void> {
                   Draft& sketch = edited->draft;
                   StoryboardCel* cel = CelStartingAt(&sketch, at);
                   if (is_over && cel == nullptr) {
                     return std::unexpected(
                         Error{Tr("That draft cel is gone.")});
                   }
                   if (cel != nullptr) {
                     cel->path = path;
                   } else {
                     sketch.cels.push_back(StoryboardCel{at, path});
                     SortCels(&sketch);
                   }
                   // A new cel shows, so Done shows what was drawn.
                   sketch.is_shown = true;
                   return {};
                 }));
    if (!applied) {
      QFile::remove(path);
    }
    return applied;
  };
  return draw_->BeginJob(std::move(job));
}

Result<void> StoryboardManager::SetShown(ShotId shot, bool is_shown) {
  assert(history_ != nullptr);
  return history_->Apply(
      is_shown ? Tr("Show draft") : Tr("Hide draft"),
      WithShot(history_->current(), shot, [is_shown](Shot* edited) {
        edited->draft.is_shown = is_shown;
        return Result<void>();
      }));
}

Result<void> StoryboardManager::SetOpacity(ShotId shot, double opacity) {
  assert(history_ != nullptr);
  assert(std::isfinite(opacity));
  const double clamped = std::clamp(opacity, 0.0, 1.0);
  return history_->Apply(
      Tr("Draft opacity"),
      WithShot(history_->current(), shot, [clamped](Shot* edited) {
        edited->draft.opacity = clamped;
        return Result<void>();
      }));
}

Result<void> StoryboardManager::MoveCels(ShotId shot, const std::set<Frame>& frames,
                                    int offset) {
  assert(history_ != nullptr);
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  if (frames.empty()) {
    return std::unexpected(Error{Tr("Pick a draft cel first.")});
  }
  auto next = WithShot(
      history_->before(), shot,
      [&frames, offset](Shot* edited) -> Result<void> {
        Draft& draft = edited->draft;
        for (StoryboardCel& cel : draft.cels) {
          if (!frames.contains(cel.at)) {
            continue;
          }
          const int moved = cel.at.index() + offset;
          if (moved < 0 || moved > kMaxFrame) {
            return std::unexpected(
                Error{Tr("A draft cel can't move before frame 0.")});
          }
          cel.at = Frame(moved);
        }
        SortCels(&draft);
        const auto clash = std::ranges::adjacent_find(
            draft.cels, [](const StoryboardCel& a, const StoryboardCel& b) {
              return a.at == b.at;
            });
        if (clash != draft.cels.end()) {
          return std::unexpected(
              Error{Tr("Another draft cel starts there already.")});
        }
        return {};
      });
  const QString label = CelLabel("Move draft cel", "Move %1 draft cels",
                                 frames.size());
  auto applied = history_->Apply(label, std::move(next));
  if (applied) {
    std::set<Frame> moved;
    for (const Frame frame : frames) {
      moved.insert(Frame(frame.index() + offset));
    }
    picked_shot_ = shot;
    picked_ = std::move(moved);
    emit Changed();
  }
  return applied;
}

Result<void> StoryboardManager::DeleteCels(ShotId shot,
                                      const std::set<Frame>& frames) {
  assert(history_ != nullptr);
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  if (frames.empty()) {
    return std::unexpected(Error{Tr("Pick a draft cel first.")});
  }
  const QString label = CelLabel("Delete draft cel", "Delete %1 draft cels",
                                 frames.size());
  auto applied = history_->Apply(
      label, WithShot(history_->current(), shot, [&frames](Shot* edited) {
        std::erase_if(edited->draft.cels, [&frames](const StoryboardCel& cel) {
          return frames.contains(cel.at);
        });
        return Result<void>();
      }));
  if (applied) {
    ClearPick();
  }
  return applied;
}

QString StoryboardManager::WhyNoDelete(ShotId shot) const {
  assert(history_ != nullptr);
  assert(picked_.size() <= static_cast<size_t>(kMaxStoryboardCels) + 1);
  return Picked(shot).empty() ? Tr("Pick a draft cel on the timeline first.")
                              : QString();
}

std::set<Frame> StoryboardManager::Picked(ShotId shot) const {
  assert(history_ != nullptr);
  const Draft* draft = Of(shot);
  std::set<Frame> picked;
  if (draft == nullptr || shot != picked_shot_) {
    return picked;
  }
  for (const Frame frame : picked_) {
    if (snapper::CelStartingAt(*draft, frame) != nullptr) {
      picked.insert(frame);
    }
  }
  return picked;
}

void StoryboardManager::Pick(ShotId shot, const std::set<Frame>& frames,
                        PickMode mode) {
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  const std::set<Frame> was = Picked(shot);
  picked_shot_ = shot;
  picked_ = Combine(was, frames, mode);
  emit Changed();
}

void StoryboardManager::ClearPick() {
  assert(picked_.size() <= static_cast<size_t>(kMaxStoryboardCels) + 1);
  const bool was_empty = picked_.empty();
  picked_.clear();
  picked_shot_ = ShotId();
  if (!was_empty) {
    emit Changed();
  }
}

}  // namespace snapper
