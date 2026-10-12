#include "edit/export_manager.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

#include "anim/master_timeline.h"
#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "io/subtitle_file.h"
#include "media/video_formats.h"
#include "edit/video_file_frames.h"
#include "media/wav_file.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

constexpr int kProgressMs = 100;

// What the worker needs, copied so the project can change meanwhile.
struct ExportJob final {
  Project project;
  ExportRequest request;
  EncodeSettings settings;
  bool is_reel = false;
  std::shared_ptr<std::atomic<bool>> cancel;
  std::shared_ptr<std::atomic<int>> done;
};

int EvenSide(int side, double scale) {
  assert(side > 0);
  assert(scale > 0.0);
  const int scaled = static_cast<int>(std::lround(side * scale / 2.0)) * 2;
  return std::max(scaled, 2);
}

// The remembered background color of the shot at master frame, which a
// format without alpha puts a see-through shot over.
QColor ShotColorAt(const Project& project, Frame master) {
  assert(master.index() >= 0);
  const ShotMoment moment = Locate(project, master);
  const bool is_past_end = moment.shot < 0;
  return is_past_end
             ? QColor(Qt::black)
             : project.shots[static_cast<size_t>(moment.shot)]->background;
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
  // This thread's own open video files.
  VideoFileFrames videos;
  // Below every reel track: clear when the format keeps alpha, so
  // see-through shot clips stay see-through where nothing is under.
  const QColor reel_under = KeepsAlpha(job.request.format)
                                ? QColor(Qt::transparent)
                                : QColor(Qt::black);
  for (int f = job.request.start.index(); f < job.request.end.index(); ++f) {
    const bool is_cancelled = job.cancel->load();
    if (is_cancelled) {
      return std::unexpected(Error{Tr("Cancelled.")});
    }
    const Frame frame(f);
    auto added = encoder.AddFrame(
        job.is_reel
            ? renderer.RenderReel(job.project, frame, job.request.scale,
                                  &videos, reel_under)
            : renderer.RenderFrame(job.project, frame, job.request.scale),
        job.is_reel ? QColor(Qt::black) : ShotColorAt(job.project, frame));
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
  const bool is_empty = ExportLength().index() == 0;
  const bool wants_song = HasSound(format) && !project.song.isEmpty();
  const bool is_song_missing = wants_song && playback_->song() == nullptr;
  const bool is_sound_loading =
      HasSound(format) && playback_->IsLoadingSound();
  const QString no_encoder = WhyNoEncoder(format);
  if (is_busy) {
    return Tr("An export is already running.");
  }
  if (!no_encoder.isEmpty()) {
    return no_encoder;
  }
  if (is_empty) {
    return Tr("Add a shot or a clip first.");
  }
  if (is_song_missing) {
    return playback_->IsLoadingSong()
               ? Tr("Wait for the song to finish loading.")
               : Tr("The song can't be read: %1")
                     .arg(playback_->song_error());
  }
  if (is_sound_loading) {
    return Tr("Wait for the sound to finish loading.");
  }
  return QString();
}

std::shared_ptr<const AudioClip> ExportManager::Mix() const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  return playback_->SoundOn(IsReelExport() ? Timeline::kReel
                                           : Timeline::kShots);
}

QString ExportManager::WhyNoSoundExport() const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  const bool is_busy = worker_.isRunning();
  if (is_busy) {
    return Tr("An export is already running.");
  }
  const bool is_loading = playback_->IsLoadingSound();
  if (is_loading) {
    return Tr("Wait for the sound to finish loading.");
  }
  const Project& project = history_->current();
  const bool is_empty = ExportLength().index() == 0 ||
                        (project.song.isEmpty() &&
                         SoundEnd(project).index() == 0);
  return is_empty ? Tr("There is no sound to export.") : QString();
}

Result<void> ExportManager::ExportSound(const QString& path) const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  const QString why_not = WhyNoSoundExport();
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  const auto mix = Mix();
  if (mix == nullptr) {
    return std::unexpected(Error{Tr("There is no sound to export.")});
  }
  const AudioClip whole = Slice(*mix, 0.0,
                                std::min(SecondsAtFrame(ExportLength()),
                                         double{kMaxAudioSeconds}));
  QSaveFile file(path);
  const bool is_written = file.open(QIODevice::WriteOnly) &&
                          file.write(WaveFile(whole)) >= 0 && file.commit();
  if (!is_written) {
    return std::unexpected(
        Error{Tr("Can't write %1: %2").arg(path, file.errorString())});
  }
  return {};
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
  const Frame total = ExportLength();
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
  // The same sound the timeline plays: the reel's mix of its unmuted
  // tracks, or each shot's sound, or the song.
  const auto mix = HasSound(range.format) ? Mix() : nullptr;
  const bool has_sound = mix != nullptr;
  if (has_sound) {
    const double length = std::min(SecondsAtFrame(range.end) -
                                       SecondsAtFrame(range.start),
                                   double{kMaxAudioSeconds});
    settings.audio = std::make_shared<const AudioClip>(
        Slice(*mix, SecondsAtFrame(range.start), length));
  }
  const bool has_subtitles = IsReelExport() && HasSubtitles();
  const bool wants_file = has_subtitles &&
                          range.subtitles != SubtitleOut::kBurnedIn;
  if (wants_file) {
    auto written = WriteSubtitles(range);
    if (!written) {
      return written;
    }
  }
  // Without burning in, the cues stay out of the picture.
  Project shown = project;
  const int subtitles = SubtitleTrack(shown.reel);
  const bool is_left_out =
      subtitles >= 0 && range.subtitles == SubtitleOut::kSideFile;
  if (is_left_out) {
    shown.reel.tracks[static_cast<size_t>(subtitles)].clips.clear();
  }
  cancel_ = std::make_shared<std::atomic<bool>>(false);
  done_ = std::make_shared<std::atomic<int>>(0);
  total_ = range.end.index() - range.start.index();
  path_ = range.path;
  worker_.setFuture(QtConcurrent::run(
      RunExport, ExportJob{std::move(shown), range, settings, IsReelExport(),
                           cancel_, done_}));
  progress_timer_.start();
  emit Progress(0, total_);
  return {};
}

bool ExportManager::HasSubtitles() const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  return !ReelManager::CuesOf(history_->current().reel).empty();
}

QString ExportManager::SubtitlePath(const QString& path, VideoFormat format) {
  assert(!path.isEmpty());
  assert(static_cast<int>(format) < kVideoFormatCount);
  const QFileInfo info(path);
  const bool is_folder = FormatSuffix(format).isEmpty();
  // A PNG frames folder gets its file beside it, named after it.
  const QString base = is_folder ? info.absoluteFilePath()
                                 : info.dir().filePath(info.completeBaseName());
  return base + (format == VideoFormat::kWebm ? QStringLiteral(".vtt")
                                              : QStringLiteral(".srt"));
}

Result<void> ExportManager::WriteSubtitles(const ExportRequest& range) const {
  assert(history_ != nullptr);
  assert(range.start < range.end);
  std::vector<SubtitleCue> cues;
  for (const SubtitleCue& cue : ReelManager::CuesOf(history_->current().reel)) {
    const bool is_in = cue.start < range.end && range.start < cue.end;
    if (is_in) {
      const int from = std::max(cue.start, range.start).index();
      const int to = std::min(cue.end, range.end).index();
      cues.push_back({Frame(from - range.start.index()),
                      Frame(to - range.start.index()), cue.text});
    }
  }
  const QString path = SubtitlePath(range.path, range.format);
  const QString text =
      range.format == VideoFormat::kWebm ? VttText(cues) : SrtText(cues);
  QSaveFile file(path);
  const bool is_written = file.open(QIODevice::WriteOnly) &&
                          file.write(text.toUtf8()) >= 0 && file.commit();
  if (!is_written) {
    return std::unexpected(
        Error{Tr("Can't write %1: %2").arg(path, file.errorString())});
  }
  return {};
}

QString ExportManager::FormatNote(VideoFormat format) const {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  const bool has_clear_shot =
      std::ranges::any_of(project.shots, [](const auto& shot) {
        return shot->is_transparent;
      });
  if (!has_clear_shot) {
    return QString();
  }
  if (!KeepsAlpha(format)) {
    return Tr("MP4 has no transparency: see-through shots get their "
              "background color.");
  }
  if (format == VideoFormat::kGif) {
    return Tr("GIF keeps one see-through color: soft edges are blended "
              "over the shot's background color.");
  }
  return IsReelExport() ? Tr("Keeps transparency where no track is under "
                             "a see-through shot.")
                        : Tr("Keeps transparency.");
}

bool ExportManager::IsReelExport() const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  return !IsReelEmpty(history_->current().reel);
}

Frame ExportManager::ExportLength() const {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  const Frame length =
      IsReelExport() ? ReelLength(project) : TotalLength(project);
  assert(length.index() >= 0);
  return length;
}

QString ExportManager::WhyNoLoop() const {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  const bool has_loop = playback_->HasLoop();
  if (!has_loop) {
    return Tr("Set a loop range first.");
  }
  const Timeline exported =
      IsReelExport() ? Timeline::kReel : Timeline::kShots;
  const bool is_elsewhere = playback_->timeline() != exported;
  if (is_elsewhere) {
    return IsReelExport() ? Tr("The loop is on the shots, not the video.")
                          : Tr("The loop is on the video, not the shots.");
  }
  return QString();
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
