#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

#include "base/frame.h"
#include "media/video_encoder.h"
#include "media/video_reader.h"

namespace snapper {
namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 48;

// Half a second of red, then half a second of blue: easy to tell apart
// after lossy encoding.
QString WriteRedThenBlue(const QTemporaryDir& dir) {
  const QString path = QDir(dir.path()).filePath("take.mp4");
  VideoEncoder encoder;
  const bool is_open =
      encoder.Open({path, VideoFormat::kMp4, kWidth, kHeight, nullptr})
          .has_value();
  for (int i = 0; is_open && i < kFramesPerSecond; ++i) {
    QImage image(kWidth, kHeight, QImage::Format_ARGB32);
    image.fill(i < kFramesPerSecond / 2 ? Qt::red : Qt::blue);
    encoder.AddFrame(image);
  }
  const bool is_done = is_open && encoder.Finish().has_value();
  return is_done ? path : QString();
}

bool IsReddish(const QImage& image) {
  const QColor middle = image.pixelColor(kWidth / 2, kHeight / 2);
  return middle.red() > 180 && middle.blue() < 80;
}

bool IsBluish(const QImage& image) {
  const QColor middle = image.pixelColor(kWidth / 2, kHeight / 2);
  return middle.blue() > 180 && middle.red() < 80;
}

}  // namespace

class VideoReaderTests final : public QObject {
  Q_OBJECT

 private slots:
  void ProbeReadsSizeAndLength();
  void PicturesFollowTime();
  void JumpingBackSeeks();
  void PastTheEndHoldsTheLast();
  void BadFilesSayWhy();
};

void VideoReaderTests::ProbeReadsSizeAndLength() {
  QTemporaryDir dir;
  const QString path = WriteRedThenBlue(dir);
  QVERIFY(!path.isEmpty());
  const auto info = ProbeVideo(path);
  QVERIFY(info.has_value());
  QCOMPARE(info->size, QSize(kWidth, kHeight));
  QVERIFY(std::abs(info->seconds - 1.0) < 0.1);
}

void VideoReaderTests::PicturesFollowTime() {
  QTemporaryDir dir;
  const QString path = WriteRedThenBlue(dir);
  VideoReader reader;
  QVERIFY(reader.Open(path).has_value());
  QCOMPARE(reader.info().size, QSize(kWidth, kHeight));
  for (int f = 0; f < kFramesPerSecond; ++f) {
    const auto picture = reader.PictureAt(SecondsAtFrame(Frame(f)));
    QVERIFY(picture.has_value());
    QCOMPARE(picture->size(), QSize(kWidth, kHeight));
    const bool is_red_half = f < kFramesPerSecond / 2;
    QVERIFY(is_red_half ? IsReddish(*picture) : IsBluish(*picture));
  }
}

void VideoReaderTests::JumpingBackSeeks() {
  QTemporaryDir dir;
  const QString path = WriteRedThenBlue(dir);
  VideoReader reader;
  QVERIFY(reader.Open(path).has_value());
  QVERIFY(IsBluish(*reader.PictureAt(0.9)));
  QVERIFY(IsReddish(*reader.PictureAt(0.1)));
  QVERIFY(IsBluish(*reader.PictureAt(0.7)));
  QVERIFY(IsReddish(*reader.PictureAt(0.0)));
}

void VideoReaderTests::PastTheEndHoldsTheLast() {
  QTemporaryDir dir;
  const QString path = WriteRedThenBlue(dir);
  VideoReader reader;
  QVERIFY(reader.Open(path).has_value());
  const auto late = reader.PictureAt(30.0);
  QVERIFY(late.has_value());
  QVERIFY(IsBluish(*late));
  // Still readable after hitting the end.
  QVERIFY(IsReddish(*reader.PictureAt(0.2)));
}

void VideoReaderTests::BadFilesSayWhy() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("junk.mp4");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("not a video at all");
  file.close();
  const auto info = ProbeVideo(path);
  QVERIFY(!info.has_value());
  QVERIFY(!info.error().message.isEmpty());
  VideoReader reader;
  QVERIFY(!reader.Open(path).has_value());
  QVERIFY(!reader.IsOpen());
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::VideoReaderTests)
#include "video_reader_tests.moc"
