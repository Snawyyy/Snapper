// The PNG sequence half of VideoEncoder: a folder of numbered frames,
// the sound beside them as a WAV.

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cassert>

#include "base/text.h"
#include "media/video_encoder.h"
#include "media/wav_file.h"

namespace snapper {
namespace {

constexpr int kNumberWidth = 5;

QString FrameName(std::int64_t index) {
  assert(index >= 0);
  return QStringLiteral("frame_%1.png")
      .arg(index, kNumberWidth, 10, QLatin1Char('0'));
}

}  // namespace

Result<void> VideoEncoder::OpenPngs() {
  assert(settings_.format == VideoFormat::kPngSequence);
  assert(!is_open_);
  const QDir folder(settings_.path);
  const bool is_file = QFileInfo(settings_.path).isFile();
  if (is_file) {
    return std::unexpected(
        Error{Tr("PNG frames go in a folder, and %1 is a file.")
                  .arg(settings_.path)});
  }
  const bool exists = folder.exists();
  const bool has_pictures =
      exists &&
      !folder.entryList({QStringLiteral("*.png")}, QDir::Files).isEmpty();
  if (has_pictures) {
    return std::unexpected(Error{
        Tr("That folder already has PNG files. Pick an empty folder.")});
  }
  const bool is_made = exists || QDir().mkpath(settings_.path);
  if (!is_made) {
    return std::unexpected(
        Error{Tr("Can't make the folder %1.").arg(settings_.path)});
  }
  made_folder_ = !exists;
  is_open_ = true;
  return {};
}

Result<void> VideoEncoder::AddPng(const QImage& frame) {
  assert(is_open_ && !is_finished_);
  assert(!frame.isNull());
  const QSize size(settings_.width, settings_.height);
  const QImage sized =
      frame.size() == size
          ? frame
          : frame.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
  const QString path = QDir(settings_.path).filePath(FrameName(frame_count_));
  const bool is_saved =
      sized.convertToFormat(QImage::Format_ARGB32).save(path, "PNG");
  if (!is_saved) {
    return std::unexpected(Error{Tr("Can't write %1.").arg(path)});
  }
  written_.append(path);
  ++frame_count_;
  return {};
}

Result<void> VideoEncoder::FinishPngs() {
  assert(is_open_ && !is_finished_);
  const bool has_sound = settings_.audio != nullptr;
  if (has_sound) {
    const QString path =
        QDir(settings_.path).filePath(QStringLiteral("sound.wav"));
    written_.append(path);
    QFile file(path);
    const bool is_written = file.open(QIODevice::WriteOnly) &&
                            file.write(WaveFile(*settings_.audio)) >= 0;
    if (!is_written) {
      return std::unexpected(Error{Tr("Can't write %1.").arg(path)});
    }
  }
  is_finished_ = true;
  return {};
}

void VideoEncoder::RemovePngs() {
  assert(settings_.format == VideoFormat::kPngSequence);
  for (const QString& path : written_) {
    QFile::remove(path);
  }
  written_.clear();
  if (made_folder_) {
    QDir().rmdir(settings_.path);
  }
}

}  // namespace snapper
