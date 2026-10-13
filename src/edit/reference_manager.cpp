#include "edit/reference_manager.h"

#include <QFileInfo>
#include <QImageReader>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "media/video_reader.h"

namespace snapper {
namespace {

// Two marks closer than this are one.
constexpr double kMarkApart = 0.5 / kFramesPerSecond;

// What a file holds: its picture size and, for a video or animated
// picture, its length.
struct Probed final {
  QSize size;
  double seconds = 0.0;
};

// A still picture is read by Qt; anything that moves goes through the
// video reader (FFmpeg reads GIF and APNG too, at their own timing).
Result<Probed> Probe(const QString& path) {
  assert(!path.isEmpty());
  QImageReader reader(path);
  const bool is_picture = reader.canRead();
  const bool is_animated = is_picture && reader.supportsAnimation() &&
                           reader.imageCount() != 1;
  const QSize picture_size = is_picture ? reader.size() : QSize();
  const bool is_still = is_picture && !is_animated && !picture_size.isEmpty();
  // An animated PNG reads here as its first frame: APNG files carry no
  // length the video reader can use.
  if (is_still) {
    return Probed{picture_size, 0.0};
  }
  const auto video = ProbeVideo(path);
  if (video) {
    return Probed{video->size, video->seconds};
  }
  // An animated picture FFmpeg can't read still shows its first frame.
  const bool is_fallback = is_picture && !picture_size.isEmpty();
  if (is_fallback) {
    return Probed{picture_size, 0.0};
  }
  return std::unexpected(Error{
      Tr("%1 is not a picture or video Snapper can read.")
          .arg(QFileInfo(path).fileName())});
}

QSize CanvasOf(const Project& project) {
  assert(IsValidCanvas(project.canvas));
  return QSize(project.canvas.width, project.canvas.height);
}

// project with reference id of shot changed by change(Reference*).
template <typename Change>
Result<Project> WithReference(Project project, ShotId shot, ReferenceId id,
                              Change change) {
  assert(shot.value() >= 0);
  assert(id.value() >= 0);
  return WithShot(std::move(project), shot,
                  [&](Shot* edited) -> Result<void> {
                    Reference* found = FindReference(&edited->references, id);
                    const bool is_present = found != nullptr;
                    if (!is_present) {
                      return std::unexpected(
                          Error{Tr("That reference no longer exists.")});
                    }
                    change(found);
                    return {};
                  });
}

}  // namespace

ReferenceManager::ReferenceManager(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  assert(are_shown_);
}

Result<ReferenceId> ReferenceManager::Add(ShotId shot,
                                          const QString& path) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_path = !path.isEmpty() && QFileInfo::exists(path);
  if (!has_path) {
    return std::unexpected(Error{Tr("That file can't be found.")});
  }
  const auto probed = Probe(path);
  if (!probed) {
    return std::unexpected(probed.error());
  }
  const QSize canvas = CanvasOf(history_->current());
  ReferenceId made;
  const auto next = WithShot(
      history_->current(), shot, [&](Shot* edited) -> Result<void> {
        const bool is_full = edited->references.size() >=
                             static_cast<size_t>(kMaxReferencesPerShot);
        if (is_full) {
          return std::unexpected(Error{
              Tr("A shot holds at most %1 references.")
                  .arg(kMaxReferencesPerShot)});
        }
        Reference reference;
        reference.id = NextReferenceId(edited->references);
        reference.path = QFileInfo(path).absoluteFilePath();
        reference.size = probed->size;
        reference.seconds = probed->seconds;
        reference.scale = std::clamp(FitFrameScale(reference, canvas),
                                     kMinReferenceScale, kMaxReferenceScale);
        made = reference.id;
        edited->references.push_back(std::move(reference));
        return {};
      });
  const auto done = history_->Apply(Tr("Add reference"), next);
  if (!done) {
    return std::unexpected(done.error());
  }
  Pick(shot, made);
  assert(made.IsValid());
  return made;
}

Result<void> ReferenceManager::Remove(ShotId shot, ReferenceId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const auto next = WithShot(
      history_->current(), shot, [id](Shot* edited) -> Result<void> {
        const auto gone = std::erase_if(
            edited->references,
            [id](const Reference& one) { return one.id == id; });
        const bool was_there = gone > 0;
        if (!was_there) {
          return std::unexpected(
              Error{Tr("That reference no longer exists.")});
        }
        return {};
      });
  const bool was_picked = PickedIn(shot) == id;
  if (was_picked) {
    Pick(shot, ReferenceId());
  }
  return history_->Apply(Tr("Remove reference"), next);
}

Result<void> ReferenceManager::Relink(ShotId shot, ReferenceId id,
                                      const QString& path) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const bool has_path = !path.isEmpty() && QFileInfo::exists(path);
  if (!has_path) {
    return std::unexpected(Error{Tr("That file can't be found.")});
  }
  const auto probed = Probe(path);
  if (!probed) {
    return std::unexpected(probed.error());
  }
  // The broken list and stills remember the old place.
  videos_.Forget();
  stills_.clear();
  const QString absolute = QFileInfo(path).absoluteFilePath();
  return history_->Apply(
      Tr("Find reference file"),
      WithReference(history_->current(), shot, id,
                    [&](Reference* reference) {
                      reference->path = absolute;
                      reference->size = probed->size;
                      reference->seconds = probed->seconds;
                      reference->skip =
                          std::clamp(reference->skip, 0.0, probed->seconds);
                    }));
}

Result<void> ReferenceManager::SetOpacity(ShotId shot, ReferenceId id,
                                          double opacity) {
  assert(history_ != nullptr);
  assert(std::isfinite(opacity));
  return history_->Apply(
      Tr("Reference opacity"),
      WithReference(history_->current(), shot, id, [opacity](Reference* r) {
        r->opacity = std::clamp(opacity, 0.0, 1.0);
      }));
}

Result<void> ReferenceManager::SetInFront(ShotId shot, ReferenceId id,
                                          bool is_in_front) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return history_->Apply(
      is_in_front ? Tr("Reference in front") : Tr("Reference behind"),
      WithReference(history_->current(), shot, id,
                    [is_in_front](Reference* r) {
                      r->is_in_front = is_in_front;
                    }));
}

Result<void> ReferenceManager::SetShown(ShotId shot, ReferenceId id,
                                        bool is_shown) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return history_->Apply(
      is_shown ? Tr("Show reference") : Tr("Hide reference"),
      WithReference(history_->current(), shot, id, [is_shown](Reference* r) {
        r->is_shown = is_shown;
      }));
}

Result<void> ReferenceManager::SetFlipped(ShotId shot, ReferenceId id,
                                          bool is_flipped) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return history_->Apply(
      Tr("Flip reference"),
      WithReference(history_->current(), shot, id,
                    [is_flipped](Reference* r) {
                      r->is_flipped = is_flipped;
                    }));
}

Result<void> ReferenceManager::SetTint(ShotId shot, ReferenceId id,
                                       ReferenceTint tint) {
  assert(history_ != nullptr);
  assert(static_cast<int>(tint) >= 0 &&
         static_cast<int>(tint) < kReferenceTintCount);
  return history_->Apply(
      Tr("Reference tint"),
      WithReference(history_->current(), shot, id,
                    [tint](Reference* r) { r->tint = tint; }));
}

Result<void> ReferenceManager::SetPlace(ShotId shot, ReferenceId id,
                                        QPointF center, double scale) {
  assert(history_ != nullptr);
  assert(std::isfinite(center.x()) && std::isfinite(center.y()));
  const bool is_usable = std::isfinite(scale) && scale > 0.0;
  if (!is_usable) {
    return std::unexpected(Error{Tr("A reference needs a size above 0.")});
  }
  return history_->Apply(
      Tr("Move reference"),
      WithReference(history_->current(), shot, id,
                    [center, scale](Reference* r) {
                      r->center = center;
                      r->scale = std::clamp(scale, kMinReferenceScale,
                                            kMaxReferenceScale);
                    }));
}

Result<void> ReferenceManager::Fit(ShotId shot, ReferenceId id,
                                   ReferenceFit fit) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const QSize canvas = CanvasOf(history_->current());
  const bool is_frame = fit == ReferenceFit::kFrame;
  return history_->Apply(
      is_frame ? Tr("Fit reference to frame") : Tr("Fit reference to height"),
      WithReference(history_->current(), shot, id,
                    [canvas, is_frame](Reference* r) {
                      const double scale = is_frame
                                               ? FitFrameScale(*r, canvas)
                                               : FitHeightScale(*r, canvas);
                      r->center = QPointF();
                      r->scale = std::clamp(scale, kMinReferenceScale,
                                            kMaxReferenceScale);
                    }));
}

Result<void> ReferenceManager::SetStart(ShotId shot, ReferenceId id,
                                        Frame start) {
  assert(history_ != nullptr);
  assert(start.index() >= 0);
  return history_->Apply(
      Tr("Reference start"),
      WithReference(history_->current(), shot, id,
                    [start](Reference* r) { r->start = start; }));
}

Result<void> ReferenceManager::SetSkip(ShotId shot, ReferenceId id,
                                       double skip) {
  assert(history_ != nullptr);
  assert(std::isfinite(skip));
  return history_->Apply(
      Tr("Slide reference"),
      WithReference(history_->current(), shot, id, [skip](Reference* r) {
        r->skip = std::clamp(skip, 0.0, r->seconds);
      }));
}

Result<void> ReferenceManager::SetSpeed(ShotId shot, ReferenceId id,
                                        double speed) {
  assert(history_ != nullptr);
  assert(std::isfinite(speed));
  return history_->Apply(
      Tr("Reference speed"),
      WithReference(history_->current(), shot, id, [speed](Reference* r) {
        r->speed = std::clamp(speed, kMinReferenceSpeed, kMaxReferenceSpeed);
      }));
}

QString ReferenceManager::WhyNoMark(ShotId shot, ReferenceId id,
                                    Frame local) const {
  assert(history_ != nullptr);
  assert(local.index() >= 0);
  const Reference* reference = Find(shot, id);
  const bool is_present = reference != nullptr;
  if (!is_present) {
    return Tr("Pick a reference row first.");
  }
  if (reference->IsStill()) {
    return Tr("A still picture has no moments to mark.");
  }
  const auto seconds = ReferenceSecondsAt(*reference, local);
  if (!seconds) {
    return Tr("The playhead is before the reference starts.");
  }
  const bool is_full =
      reference->marks.size() >= static_cast<size_t>(kMaxReferenceMarks);
  if (is_full) {
    return Tr("The reference has as many markers as it can hold.");
  }
  const bool is_marked = std::ranges::any_of(
      reference->marks, [&seconds](double mark) {
        return std::abs(mark - *seconds) < kMarkApart;
      });
  return is_marked ? Tr("There is a marker here already.") : QString();
}

Result<void> ReferenceManager::AddMark(ShotId shot, ReferenceId id,
                                       Frame local) {
  assert(history_ != nullptr);
  assert(local.index() >= 0);
  const QString why = WhyNoMark(shot, id, local);
  const bool is_refused = !why.isEmpty();
  if (is_refused) {
    return std::unexpected(Error{why});
  }
  return history_->Apply(
      Tr("Reference marker"),
      WithReference(history_->current(), shot, id, [local](Reference* r) {
        r->marks.push_back(*ReferenceSecondsAt(*r, local));
        std::ranges::sort(r->marks);
      }));
}

Result<void> ReferenceManager::ClearMarks(ShotId shot, ReferenceId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return history_->Apply(
      Tr("Clear reference markers"),
      WithReference(history_->current(), shot, id,
                    [](Reference* r) { r->marks.clear(); }));
}

std::vector<Reference> ReferenceManager::Of(ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr ? found->references : std::vector<Reference>();
}

const Reference* ReferenceManager::Find(ShotId shot, ReferenceId id) const {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  return found != nullptr ? FindReference(found->references, id) : nullptr;
}

bool ReferenceManager::IsMissing(const Reference& reference) {
  assert(reference.id.value() >= 0);
  assert(reference.speed > 0.0);
  return reference.path.isEmpty() || !QFileInfo::exists(reference.path);
}

ReferenceId ReferenceManager::PickedIn(ShotId shot) const {
  assert(shot.value() >= 0);
  assert(picked_.value() >= 0);
  const bool is_here = shot.IsValid() && shot == picked_shot_ &&
                       Find(shot, picked_) != nullptr;
  return is_here ? picked_ : ReferenceId();
}

void ReferenceManager::Pick(ShotId shot, ReferenceId id) {
  assert(shot.value() >= 0);
  assert(id.value() >= 0);
  const bool is_change = shot != picked_shot_ || id != picked_;
  if (!is_change) {
    return;
  }
  picked_shot_ = shot;
  picked_ = id;
  emit Changed();
}

void ReferenceManager::ToggleShown() {
  const bool was = are_shown_;
  are_shown_ = !are_shown_;
  emit Changed();
  assert(are_shown_ != was);
}

bool ReferenceManager::IsVisible(const Reference& reference) const {
  assert(reference.speed > 0.0);
  assert(reference.opacity >= 0.0);
  return are_shown_ && reference.is_shown && reference.opacity > 0.0 &&
         !IsMissing(reference);
}

QImage ReferenceManager::Picture(const Reference& reference, Frame local) {
  assert(reference.speed > 0.0);
  assert(local.index() >= 0);
  const auto seconds = ReferenceSecondsAt(reference, local);
  const bool is_drawn = seconds.has_value() && IsVisible(reference);
  if (!is_drawn) {
    return QImage();
  }
  if (!reference.IsStill()) {
    return videos_.PictureAtSeconds(reference.path, *seconds);
  }
  const auto known = stills_.find(reference.path);
  const bool is_known = known != stills_.end();
  if (is_known) {
    return known->second;
  }
  QImageReader reader(reference.path);
  const QImage picture = reader.read();
  stills_[reference.path] = picture;
  return picture;
}

}  // namespace snapper
