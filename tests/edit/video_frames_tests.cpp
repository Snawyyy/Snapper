#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "edit/video_file_frames.h"
#include "media/video_encoder.h"

namespace snapper {
namespace {

// A second of video, red for frames before 12 and blue after.
QString WriteTake(const QTemporaryDir& dir, const QString& name) {
  const QString path = QDir(dir.path()).filePath(name);
  VideoEncoder encoder;
  const bool is_open =
      encoder.Open({path, VideoFormat::kMp4, 32, 32, nullptr}).has_value();
  for (int i = 0; is_open && i < kFramesPerSecond; ++i) {
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(i < kFramesPerSecond / 2 ? Qt::red : Qt::blue);
    encoder.AddFrame(image);
  }
  const bool is_done = is_open && encoder.Finish().has_value();
  return is_done ? path : QString();
}

}  // namespace

class VideoFramesTests final : public QObject {
  Q_OBJECT

 private slots:
  void ReadsPicturesByFrame();
  void BrokenFilesDrawNothingAndAreRemembered();
  void ClosesTheOldestPastTheLimit();
};

void VideoFramesTests::ReadsPicturesByFrame() {
  QTemporaryDir dir;
  const QString path = WriteTake(dir, "take.mp4");
  VideoFileFrames frames;
  const QImage early = frames.Picture(path, Frame(2));
  const QImage late = frames.Picture(path, Frame(20));
  QVERIFY(!early.isNull() && !late.isNull());
  QVERIFY(early.pixelColor(16, 16).red() > 180);
  QVERIFY(late.pixelColor(16, 16).blue() > 180);
  QVERIFY(frames.broken().empty());
}

void VideoFramesTests::BrokenFilesDrawNothingAndAreRemembered() {
  QTemporaryDir dir;
  VideoFileFrames frames;
  const QString missing = QDir(dir.path()).filePath("gone.mp4");
  QVERIFY(frames.Picture(missing, Frame(0)).isNull());
  QVERIFY(frames.broken().contains(missing));
  QVERIFY(frames.Picture(QString(), Frame(0)).isNull());
  frames.Forget();
  QVERIFY(frames.broken().empty());
}

void VideoFramesTests::ClosesTheOldestPastTheLimit() {
  QTemporaryDir dir;
  const QString first = WriteTake(dir, "take.mp4");
  VideoFileFrames frames;
  for (int i = 0; i <= kMaxOpenVideos; ++i) {
    const QString copy = QDir(dir.path()).filePath(QString("t%1.mp4").arg(i));
    QVERIFY(QFile::copy(first, copy));
    QVERIFY(!frames.Picture(copy, Frame(0)).isNull());
  }
  // The first one was closed and opens again on demand.
  QVERIFY(!frames.Picture(QDir(dir.path()).filePath("t0.mp4"), Frame(20))
               .isNull());
  QVERIFY(frames.broken().empty());
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::VideoFramesTests)
#include "video_frames_tests.moc"
