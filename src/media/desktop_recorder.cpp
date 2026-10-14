#include "media/desktop_recorder.h"

#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include "base/text.h"

namespace snapper {
namespace {

constexpr int kRate = 48000;
constexpr int kChannels = 2;
constexpr int kBytesPerFrame = kChannels * 2;
constexpr int kStartWaitMs = 2000;
constexpr int kStopWaitMs = 1000;
constexpr double kShortMax = 32768.0;

// parec without a latency hint buffers, and a short take comes back
// empty.
QStringList ParecArguments() {
  assert(kRate > 0);
  assert(kChannels > 0);
  return {QStringLiteral("-d"),
          QStringLiteral("@DEFAULT_MONITOR@"),
          QStringLiteral("--latency-msec=50"),
          QStringLiteral("--raw"),
          QStringLiteral("--format=s16le"),
          QStringLiteral("--rate=%1").arg(kRate),
          QStringLiteral("--channels=%1").arg(kChannels)};
}

}  // namespace

DesktopRecorder::DesktopRecorder() {
  assert(take_.samples.empty());
  assert(!IsRecording());
  process_.setStandardErrorFile(QProcess::nullDevice());
  connect(&process_, &QProcess::readyReadStandardOutput, this,
          &DesktopRecorder::Read);
}

DesktopRecorder::~DesktopRecorder() {
  assert(kRate > 0);
  assert(kBytesPerFrame == 4);
  End();
}

QString DesktopRecorder::WhyNoDesktop() {
  assert(kChannels > 0);
  assert(kRate > 0);
  const bool has_parec =
      !QStandardPaths::findExecutable(QStringLiteral("parec")).isEmpty();
  return has_parec ? QString()
                   : Tr("Recording PC audio needs parec (the "
                        "pulseaudio-utils package).");
}

Result<void> DesktopRecorder::Start() {
  assert(kStartWaitMs > 0);
  assert(kMaxSampleSeconds > 0);
  const bool is_busy = IsRecording();
  if (is_busy) {
    return std::unexpected(Error{Tr("PC audio is already recording.")});
  }
  const QString why_not = WhyNoDesktop();
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  take_ = Take{kRate, kChannels, {}};
  rest_.clear();
  process_.start(QStringLiteral("parec"), ParecArguments());
  const bool is_started = process_.waitForStarted(kStartWaitMs);
  if (!is_started) {
    return std::unexpected(Error{Tr("Could not start parec to record the "
                                    "PC's sound.")});
  }
  return {};
}

void DesktopRecorder::Read() {
  assert(take_.channels == kChannels || take_.channels == 0);
  assert(kBytesPerFrame > 0);
  rest_.append(process_.readAllStandardOutput());
  const size_t room = static_cast<size_t>(kMaxSampleSeconds) * kRate *
                      kChannels;
  const size_t old = take_.samples.size();
  const size_t heard = static_cast<size_t>(rest_.size() / kBytesPerFrame) *
                       kChannels;
  const size_t kept = std::min(heard, room - std::min(room, old));
  take_.samples.resize(old + kept);
  std::memcpy(take_.samples.data() + old, rest_.constData(), kept * 2);
  rest_.remove(0, static_cast<qsizetype>(heard * 2));
  int peak = 0;
  for (size_t i = old; i < old + kept; ++i) {
    peak = std::max(peak, std::abs(static_cast<int>(take_.samples[i])));
  }
  emit Level(peak / kShortMax);
  emit Seconds(take_.Seconds());
  const bool is_full = take_.samples.size() >= room;
  const bool is_over = is_full && IsRecording();
  if (is_over) {
    End();
    emit LimitReached();
  }
}

void DesktopRecorder::End() {
  assert(kStopWaitMs > 0);
  assert(kStartWaitMs > 0);
  const bool is_running = IsRecording();
  if (!is_running) {
    return;
  }
  process_.terminate();
  const bool is_ended = process_.waitForFinished(kStopWaitMs);
  if (!is_ended) {
    process_.kill();
    process_.waitForFinished(kStopWaitMs);
  }
}

Take DesktopRecorder::Stop() {
  assert(take_.rate >= 0);
  assert(kStopWaitMs > 0);
  End();
  // What parec wrote before it ended is still waiting in the pipe.
  Read();
  Take done = std::move(take_);
  take_ = Take{};
  return done;
}

}  // namespace snapper
