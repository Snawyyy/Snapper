#include "edit/sound_manager.h"

#include <QFileInfo>
#include <QStringList>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <utility>
#include <variant>

#include "anim/master_timeline.h"
#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

static_assert(kAudioRate % kFramesPerSecond == 0);
constexpr std::int64_t kSamplesPerFrame = kAudioRate / kFramesPerSecond;
constexpr double kMaxDb = 6.0;

std::int64_t SamplesOf(Frame frame) {
  assert(frame.index() >= 0);
  assert(kSamplesPerFrame > 0);
  return static_cast<std::int64_t>(frame.index()) * kSamplesPerFrame;
}

// Silence for length frames, no longer than any sound can be.
AudioClip Silence(Frame length) {
  assert(length.index() >= 0);
  assert(kMaxAudioSeconds > 0);
  const std::int64_t most =
      static_cast<std::int64_t>(kMaxAudioSeconds) * kAudioRate;
  AudioClip clip;
  clip.samples.assign(
      static_cast<size_t>(std::min(SamplesOf(length), most) * kAudioChannels),
      0.0f);
  return clip;
}

// The gain of the fades sample i of count into a clip is under.
double FadeAt(std::int64_t i, std::int64_t count, std::int64_t fade_in,
              std::int64_t fade_out) {
  assert(i >= 0 && i < count);
  assert(fade_in >= 0 && fade_out >= 0);
  double gain = 1.0;
  const bool is_fading_in = i < fade_in;
  if (is_fading_in) {
    gain *= (static_cast<double>(i) + 0.5) / static_cast<double>(fade_in);
  }
  const std::int64_t left = count - i;
  const bool is_fading_out = left <= fade_out;
  if (is_fading_out) {
    gain *= (static_cast<double>(left) - 0.5) / static_cast<double>(fade_out);
  }
  return gain;
}

// Adds clip's sound, at gain, into mix (which starts at reel frame 0).
void MixClip(const AudioClip& sound, const Clip& clip, double gain,
             AudioClip* mix) {
  assert(mix != nullptr);
  assert(gain >= 0.0);
  const std::int64_t first = SamplesOf(clip.start);
  const std::int64_t count = SamplesOf(clip.length);
  const std::int64_t in = SamplesOf(clip.in);
  const std::int64_t fade_in = SamplesOf(clip.fade_in);
  const std::int64_t fade_out = SamplesOf(clip.fade_out);
  const std::int64_t room = mix->FrameCount();
  const std::int64_t have = sound.FrameCount();
  const std::int64_t last =
      std::min({count, room - first, have - in});
  for (std::int64_t i = 0; i < last; ++i) {
    const auto g =
        static_cast<float>(gain * FadeAt(i, count, fade_in, fade_out));
    const auto to = static_cast<size_t>((first + i) * kAudioChannels);
    const auto from = static_cast<size_t>((in + i) * kAudioChannels);
    for (size_t c = 0; c < static_cast<size_t>(kAudioChannels); ++c) {
      mix->samples[to + c] += sound.samples[from + c] * g;
    }
  }
}

// Copies count samples of from (at from_at) into to (at to_at), silent
// past from's end.
void CopySamples(const AudioClip& from, std::int64_t from_at,
                 std::int64_t count, std::int64_t to_at, AudioClip* to) {
  assert(to != nullptr);
  assert(count >= 0);
  const std::int64_t room = to->FrameCount() - to_at;
  const std::int64_t have = from.FrameCount() - from_at;
  const std::int64_t last = std::min({count, room, have});
  const bool has_any = last > 0 && from_at >= 0 && to_at >= 0;
  if (!has_any) {
    return;
  }
  std::copy_n(from.samples.begin() + from_at * kAudioChannels,
              last * kAudioChannels,
              to->samples.begin() + to_at * kAudioChannels);
}

}  // namespace

bool IsSoundFile(const QString& path) {
  assert(path.size() < 100000);
  static const QStringList kSuffixes = {
      QStringLiteral("wav"), QStringLiteral("mp3"),  QStringLiteral("ogg"),
      QStringLiteral("flac"), QStringLiteral("m4a"), QStringLiteral("aac"),
      QStringLiteral("opus")};
  return kSuffixes.contains(QFileInfo(path).suffix().toLower());
}

double GainToDb(double gain) {
  assert(std::isfinite(gain));
  assert(gain >= 0.0);
  return gain <= 0.0 ? -std::numeric_limits<double>::infinity()
                     : 20.0 * std::log10(gain);
}

QString GainText(double gain) {
  assert(std::isfinite(gain));
  const double db = GainToDb(gain);
  const bool is_silent = db < kQuietestDb;
  if (is_silent) {
    return Tr("Silent");
  }
  const double shown = std::round(db * 10.0) / 10.0;
  return QStringLiteral("%1%2 dB")
      .arg(shown > 0.0 ? QStringLiteral("+") : QString())
      .arg(shown, 0, 'f', 1);
}

double GainToLevel(double gain) {
  assert(std::isfinite(gain));
  assert(kMaxDb > kQuietestDb);
  const double db = GainToDb(gain);
  const bool is_silent = db < kQuietestDb;
  return is_silent ? 0.0
                   : std::clamp((db - kQuietestDb) / (kMaxDb - kQuietestDb),
                                0.0, 1.0);
}

double LevelToGain(double level) {
  assert(std::isfinite(level));
  assert(kMaxDb > kQuietestDb);
  const double kept = std::clamp(level, 0.0, 1.0);
  const bool is_silent = kept <= 0.0;
  return is_silent ? 0.0
                   : std::min(std::pow(10.0, (kQuietestDb + kept * (kMaxDb -
                                                                    kQuietestDb)) /
                                                 20.0),
                              kMaxGain);
}

SoundManager::SoundManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  connect(history_, &HistoryManager::Changed, this, &SoundManager::Follow);
  Follow();
  assert(preferred_.empty());
}

SoundManager::~SoundManager() {
  assert(history_ != nullptr);
  // A decode still running must finish before its watcher goes.
  for (auto& [path, entry] : sounds_) {
    if (entry.loader != nullptr) {
      entry.loader->waitForFinished();
    }
  }
}

std::shared_ptr<const AudioClip> SoundManager::Sound(
    const QString& path) const {
  assert(history_ != nullptr);
  const auto found = sounds_.find(path);
  return found == sounds_.end() ? nullptr : found->second.clip;
}

QString SoundManager::ErrorOf(const QString& path) const {
  assert(history_ != nullptr);
  const auto found = sounds_.find(path);
  return found == sounds_.end() ? QString() : found->second.error;
}

bool SoundManager::IsLoading() const {
  assert(history_ != nullptr);
  return std::ranges::any_of(sounds_, [](const auto& item) {
    return item.second.loader != nullptr && item.second.loader->isRunning();
  });
}

float SoundManager::PeakBetween(const QString& path, double from,
                                double to) const {
  assert(std::isfinite(from) && std::isfinite(to));
  assert(kPeaksPerSecond > 0);
  const auto found = sounds_.find(path);
  const bool is_loaded = found != sounds_.end() && !found->second.peaks.empty();
  if (!is_loaded) {
    return 0.0f;
  }
  const std::vector<float>& peaks = found->second.peaks;
  const auto count = static_cast<std::int64_t>(peaks.size());
  const auto first = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(std::floor(from * kPeaksPerSecond)), 0, count);
  const auto last = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(std::ceil(to * kPeaksPerSecond)), first + 1,
      count);
  float peak = 0.0f;
  for (std::int64_t i = first; i < last; ++i) {
    peak = std::max(peak, peaks[static_cast<size_t>(i)]);
  }
  return peak;
}

AudioClip SoundManager::MixReel(const Project& project, Frame length) const {
  assert(history_ != nullptr);
  assert(length.index() >= 0);
  AudioClip mix = Silence(length);
  const Reel& reel = project.reel;
  const int count = static_cast<int>(reel.tracks.size());
  for (int t = 0; t < count; ++t) {
    const bool is_heard = IsTrackHeard(reel, t);
    if (!is_heard) {
      continue;
    }
    const ReelTrack& track = reel.tracks[static_cast<size_t>(t)];
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<SoundSource>(&clip.source);
      const auto sound =
          source != nullptr ? Sound(source->path) : nullptr;
      if (sound != nullptr) {
        MixClip(*sound, clip, clip.volume * track.volume, &mix);
      }
    }
  }
  for (float& sample : mix.samples) {
    sample = std::clamp(sample, -1.0f, 1.0f);
  }
  return mix;
}

std::vector<ClipId> SoundManager::ClipsOf(const Project& project,
                                          ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  std::vector<const Clip*> found;
  for (const ReelTrack& track : project.reel.tracks) {
    for (const Clip& clip : track.clips) {
      const auto* source = std::get_if<ShotSource>(&clip.source);
      const bool is_its = source != nullptr && source->shot == shot;
      if (is_its) {
        found.push_back(&clip);
      }
    }
  }
  std::ranges::sort(found, [](const Clip* a, const Clip* b) {
    return a->start < b->start;
  });
  std::vector<ClipId> ids;
  for (const Clip* clip : found) {
    ids.push_back(clip->id);
  }
  return ids;
}

ClipId SoundManager::SoundClipOf(const Project& project, ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const std::vector<ClipId> clips = ClipsOf(project, shot);
  if (clips.empty()) {
    return ClipId();
  }
  const auto preferred = preferred_.find(shot);
  const bool has_preferred =
      preferred != preferred_.end() &&
      std::ranges::find(clips, preferred->second) != clips.end();
  return has_preferred ? preferred->second : clips.front();
}

void SoundManager::PreferClip(ClipId clip) {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const Clip* found = ClipOf(history_->current().reel, clip);
  const auto* source =
      found != nullptr ? std::get_if<ShotSource>(&found->source) : nullptr;
  if (source == nullptr) {
    return;
  }
  const auto known = preferred_.find(source->shot);
  const bool is_new = known == preferred_.end() || known->second != clip;
  if (is_new) {
    preferred_[source->shot] = clip;
    emit SoundsChanged();
  }
}

std::shared_ptr<const AudioClip> SoundManager::MixShots(
    const Project& project, const AudioClip* song) const {
  assert(history_ != nullptr);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const bool is_any_placed = std::ranges::any_of(
      project.shots, [&](const auto& shot) {
        return SoundClipOf(project, shot->id).IsValid();
      });
  if (!is_any_placed) {
    return nullptr;
  }
  const AudioClip reel =
      MixReel(project, std::max(ReelLength(project), SoundEnd(project)));
  auto mix = std::make_shared<AudioClip>(Silence(TotalLength(project)));
  const int count = static_cast<int>(project.shots.size());
  for (int i = 0; i < count; ++i) {
    const Shot& shot = *project.shots[static_cast<size_t>(i)];
    const std::int64_t at = SamplesOf(ShotStart(project, i));
    const ClipId borrowed = SoundClipOf(project, shot.id);
    const Clip* clip = ClipOf(project.reel, borrowed);
    if (clip == nullptr) {
      if (song != nullptr) {
        CopySamples(*song, at, SamplesOf(shot.length), at, mix.get());
      }
      continue;
    }
    // The clip's frames of the shot play what is under the clip.
    const Frame inside(std::min(clip->length.index(),
                                shot.length.index() - clip->in.index()));
    CopySamples(reel, SamplesOf(clip->start), SamplesOf(inside),
                at + SamplesOf(clip->in), mix.get());
  }
  return mix;
}

void SoundManager::Follow() {
  assert(history_ != nullptr);
  std::set<QString> paths;
  for (const ReelTrack& track : history_->current().reel.tracks) {
    for (const Clip& clip : track.clips) {
      const auto* sound = std::get_if<SoundSource>(&clip.source);
      if (sound != nullptr) {
        paths.insert(sound->path);
      }
    }
  }
  for (const QString& path : paths) {
    const bool is_known = sounds_.contains(path);
    if (is_known) {
      continue;
    }
    Entry& entry = sounds_[path];
    entry.loader = std::make_unique<QFutureWatcher<Result<AudioClip>>>();
    connect(entry.loader.get(),
            &QFutureWatcher<Result<AudioClip>>::finished, this,
            [this, path] { Loaded(path); });
    entry.loader->setFuture(QtConcurrent::run(DecodeAudio, path));
  }
}

void SoundManager::Loaded(const QString& path) {
  assert(history_ != nullptr);
  assert(sounds_.contains(path));
  Entry& entry = sounds_.at(path);
  assert(entry.loader != nullptr && entry.loader->isFinished());
  Result<AudioClip> loaded = entry.loader->result();
  if (loaded) {
    const int count = static_cast<int>(
        std::ceil(loaded->Seconds() * kPeaksPerSecond));
    entry.peaks = Peaks(*loaded, std::max(count, 1));
    entry.clip = std::make_shared<const AudioClip>(std::move(*loaded));
  } else {
    entry.error = loaded.error().message;
  }
  emit SoundsChanged();
}

}  // namespace snapper
