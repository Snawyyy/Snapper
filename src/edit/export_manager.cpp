#include "edit/export_manager.h"

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/master_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

constexpr int kProgressMs = 100;

// What the worker needs, copied so the project can change meanwhile.
struct ExportJob final {
  Project project;
  ExportRequest request;
  EncodeSettings settings;
  std::shared_ptr<std::atomic<bool>> cancel;
  std::shared_ptr<std::atomic<int>> done;
};

int EvenSide(int side, double scale) {
  assert(side > 0);
  assert(scale > 0.0);
  const int scaled = static_cast<int>(std::lround(side * scale / 2.0)) * 2;
  return std::max(scaled, 2);
}

// Runs on the worker thread: nothing here touches the managers.
Result<void> RunExport(ExportJob job) {
  assert(job.cancel != nullptr && job.done != nullptr);
  assert(job.request.start < job.request.end);
  VideoEncoder encoder;
  auto opened = encoder.Open(job.settings);
  if (!opened) {
    return opened;
  }
  FrameRenderer renderer;
  for (int f = job.request.start.index(); f < job.request.end.index(); ++f) {
    const bool is_cancelled = job.cancel->load();
    if (is_cancelled) {
      return std::unexpected(Error{Tr("Cancelled.")});
    }
    auto added = encoder.AddFrame(
        renderer.RenderFrame(job.project, Frame(f), job.request.scale));
    if (!added) {
      return added;
    }
    job.done->fetch_add(1);
  }
  return encoder.Finish();
}

}  // namespace

ExportManager::ExportManager(HistoryManager* history,
                             PlaybackManager* playback)
    : history_(history), playback_(playback) {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  progress_timer_.setInterval(kProgressMs);
  connect(&progress_timer_, &QTimer::timeout, this, &ExportManager::Report);
  connect(&worker_, &QFutureWatcher<Result<void>>::finished, this,
          &ExportManager::Done);
}

ExportManager::~ExportManager() {
  assert(history_ != nullptr);
  Cancel();
  worker_.waitForFinished();
  assert(!worker_.isRunning());
}

QString ExportManager::WhyNoExport(VideoFormat format) const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  const Project& project = history_->current();
  const bool is_busy = worker_.isRunning();
  const bool is_empty = TotalLength(project).index() == 0;
  const bool wants_song =
      format == VideoFormat::kMp4 && !project.song.isEmpty();
  const bool is_song_missing = wants_song && playback_->song() == nullptr;
  if (is_busy) {
    return Tr("An export is already running.");
  }
  if (is_empty) {
    return Tr("Add a shot first.");
  }
  if (is_song_missing) {
    return playback_->IsLoadingSong()
               ? Tr("Wait for the song to finish loading.")
               : Tr("The song can't be read: %1")
                     .arg(playback_->song_error());
  }
  return QString();
}

Result<void> ExportManager::Start(const ExportRequest& request) {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  const QString why_not = WhyNoExport(request.format);
  const bool can_start = why_not.isEmpty();
  if (!can_start) {
    return std::unexpected(Error{why_not});
  }
  const Project& project = history_->current();
  ExportRequest range = request;
  const Frame total = TotalLength(project);
  range.end = range.end.index() == 0 ? total : std::min(range.end, total);
  const bool is_valid = !range.path.isEmpty() && range.start < range.end &&
                        range.scale > 0.0 && range.scale <= 1.0;
  if (!is_valid) {
    return std::unexpected(
        Error{Tr("Pick a file and a range with at least one frame.")});
  }
  EncodeSettings settings{range.path, range.format,
                          EvenSide(project.canvas.width, range.scale),
                          EvenSide(project.canvas.height, range.scale),
                          nullptr};
  const bool has_song =
      range.format == VideoFormat::kMp4 && playback_->song() != nullptr;
  if (has_song) {
    const double length = SecondsAtFrame(range.end) -
                          SecondsAtFrame(range.start);
    settings.audio = std::make_shared<const AudioClip>(Slice(
        *playback_->song(), SecondsAtFrame(range.start), length));
  }
  cancel_ = std::make_shared<std::atomic<bool>>(false);
  done_ = std::make_shared<std::atomic<int>>(0);
  total_ = range.end.index() - range.start.index();
  path_ = range.path;
  worker_.setFuture(QtConcurrent::run(
      RunExport, ExportJob{project, range, settings, cancel_, done_}));
  progress_timer_.start();
  emit Progress(0, total_);
  return {};
}

void ExportManager::Cancel() {
  assert(history_ != nullptr);
  const bool is_running = worker_.isRunning() && cancel_ != nullptr;
  if (is_running) {
    cancel_->store(true);
  }
  assert(!is_running || cancel_->load());
}

void ExportManager::Report() {
  assert(done_ != nullptr);
  assert(total_ > 0);
  emit Progress(done_->load(), total_);
}

void ExportManager::Done() {
  assert(worker_.isFinished());
  assert(cancel_ != nullptr);
  progress_timer_.stop();
  Report();
  const Result<void> result = worker_.result();
  const bool is_cancelled = cancel_->load();
  if (is_cancelled) {
    emit Cancelled();
  } else if (result) {
    emit Finished(path_);
  } else {
    emit Failed(result.error().message);
  }
}

}  // namespace snapper
