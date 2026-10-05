#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <cmath>
#include <numbers>

#include "base/frame.h"
#include "media/audio_player.h"
#include "media/video_encoder.h"

namespace snapper {
namespace {

// A sine tone, loud enough to find again after encoding.
std::shared_ptr<const AudioClip> Tone(double seconds) {
  AudioClip clip;
  const int frames = static_cast<int>(seconds * kAudioRate);
  for (int i = 0; i < frames; ++i) {
    const auto value = static_cast<float>(
        0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * i / kAudioRate));
    clip.samples.push_back(value);
    clip.samples.push_back(value);
  }
  return std::make_shared<const AudioClip>(clip);
}

QImage Solid(QColor color) {
  QImage image(64, 48, QImage::Format_ARGB32);
  image.fill(color);
  return image;
}

struct Probe final {
  int streams = 0;
  AVCodecID video = AV_CODEC_ID_NONE;
  int width = 0;
  double seconds = 0.0;
};

Probe ProbeFile(const QString& path) {
  Probe probe;
  AVFormatContext* raw = nullptr;
  const QByteArray name = path.toUtf8();
  const bool is_open =
      avformat_open_input(&raw, name.constData(), nullptr, nullptr) >= 0 &&
      avformat_find_stream_info(raw, nullptr) >= 0;
  const InputHandle input(raw);
  if (!is_open) {
    return probe;
  }
  probe.streams = static_cast<int>(raw->nb_streams);
  for (unsigned i = 0; i < raw->nb_streams; ++i) {
    const AVCodecParameters* codec = raw->streams[i]->codecpar;
    const bool is_video = codec->codec_type == AVMEDIA_TYPE_VIDEO;
    if (is_video) {
      probe.video = codec->codec_id;
      probe.width = codec->width;
    }
  }
  probe.seconds = static_cast<double>(raw->duration) / AV_TIME_BASE;
  return probe;
}

Result<void> EncodeSecond(const EncodeSettings& settings) {
  VideoEncoder encoder;
  auto opened = encoder.Open(settings);
  if (!opened) {
    return opened;
  }
  for (int i = 0; i < kFramesPerSecond; ++i) {
    auto added = encoder.AddFrame(Solid(i % 2 == 0 ? Qt::red : Qt::blue));
    if (!added) {
      return added;
    }
  }
  return encoder.Finish();
}

}  // namespace

class MediaTests final : public QObject {
  Q_OBJECT

 private slots:
  void Mp4KeepsPictureAndSound();
  void GifHasEveryFrame();
  void CancelledExportLeavesNoFile();
  void PeaksFollowLoudness();
  void SlicesPadWithSilence();
  void SourceReadsFromTheSeekPoint();
  void SilentClockStillRuns();
  void BadFilesSayWhy();
};

void MediaTests::Mp4KeepsPictureAndSound() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("out.mp4");
  const auto encoded =
      EncodeSecond({path, VideoFormat::kMp4, 64, 48, Tone(1.0)});
  QVERIFY(encoded.has_value());
  const Probe probe = ProbeFile(path);
  QCOMPARE(probe.streams, 2);
  QCOMPARE(probe.width, 64);
  QVERIFY(std::abs(probe.seconds - 1.0) < 0.1);
  const auto sound = DecodeAudio(path);
  QVERIFY(sound.has_value());
  QVERIFY(std::abs(sound->Seconds() - 1.0) < 0.1);
  QVERIFY(Peaks(*sound, 1)[0] > 0.3f);
}

void MediaTests::GifHasEveryFrame() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("out.gif");
  QVERIFY(EncodeSecond({path, VideoFormat::kGif, 64, 48, nullptr})
              .has_value());
  const Probe probe = ProbeFile(path);
  QCOMPARE(probe.streams, 1);
  QCOMPARE(probe.video, AV_CODEC_ID_GIF);
  QVERIFY(std::abs(probe.seconds - 1.0) < 0.1);
}

void MediaTests::CancelledExportLeavesNoFile() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("half.mp4");
  {
    VideoEncoder encoder;
    QVERIFY(encoder.Open({path, VideoFormat::kMp4, 64, 48, nullptr})
                .has_value());
    QVERIFY(encoder.AddFrame(Solid(Qt::green)).has_value());
    QVERIFY(QFile::exists(path));
  }
  QVERIFY(!QFile::exists(path));
}

void MediaTests::PeaksFollowLoudness() {
  AudioClip clip;
  clip.samples = {0.1f, -0.2f, 0.0f, 0.0f, 0.9f, 0.0f, -1.5f, 0.0f};
  const std::vector<float> peaks = Peaks(clip, 2);
  QCOMPARE(peaks.size(), size_t{2});
  QCOMPARE(peaks[0], 0.2f);
  QCOMPARE(peaks[1], 1.0f);
  QCOMPARE(Peaks(AudioClip(), 3), std::vector<float>(3, 0.0f));
}

void MediaTests::SlicesPadWithSilence() {
  const auto tone = Tone(1.0);
  const AudioClip slice = Slice(*tone, 0.5, 1.0);
  QCOMPARE(slice.FrameCount(), kAudioRate);
  QCOMPARE(slice.samples.back(), 0.0f);
  QCOMPARE(slice.samples[0], tone->samples[kAudioRate]);
  QCOMPARE(Slice(*tone, -1.0, 0.5).samples.front(), 0.0f);
}

void MediaTests::SourceReadsFromTheSeekPoint() {
  AudioClip clip;
  clip.samples = {1.0f, 2.0f, 3.0f, 4.0f};
  PcmSource source(std::make_shared<const AudioClip>(clip));
  source.Seek(1.0 / kAudioRate);
  QCOMPARE(source.bytesAvailable(), qint64{8});
  float read[2] = {};
  QCOMPARE(source.read(reinterpret_cast<char*>(read), 64), qint64{8});
  QCOMPARE(read[0], 3.0f);
  QCOMPARE(source.read(reinterpret_cast<char*>(read), 8), qint64{0});
}

void MediaTests::SilentClockStillRuns() {
  AudioPlayer player;
  player.Play(2.0);
  QVERIFY(player.IsPlaying());
  QThread::msleep(30);
  const double now = player.Position();
  QVERIFY(now > 2.01 && now < 3.0);
  player.Stop();
  const double stopped = player.Position();
  QThread::msleep(10);
  QCOMPARE(player.Position(), stopped);
}

void MediaTests::BadFilesSayWhy() {
  QTemporaryDir dir;
  const QString path = QDir(dir.path()).filePath("noise.mp3");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("not sound at all");
  file.close();
  const auto clip = DecodeAudio(path);
  QVERIFY(!clip.has_value());
  QVERIFY(!clip.error().message.isEmpty());
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::MediaTests)
#include "media_tests.moc"
