#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

#include "edit/export_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "media/video_reader.h"

namespace snapper {
namespace {

Project Lasting(int length) {
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(length);
  shot.background = Qt::magenta;
  Project project;
  project.canvas = {64, 48};
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

// Lasting(length) on the reel from frame 6, after six black frames.
Project LateOnTheReel(int length) {
  Project project = Lasting(length);
  Clip clip;
  clip.id = ClipId(1);
  clip.source = ShotSource{ShotId(1)};
  clip.start = Frame(6);
  clip.length = Frame(length);
  PlaceClip(&project.reel.tracks[1], clip);
  return project;
}

}  // namespace

class ExportTests final : public QObject {
  Q_OBJECT

 private slots:
  void ExportsMp4AndGif();
  void CancellingLeavesNoFile();
  void ExportsTheReelOnceItHasClips();
  void LoopMustBeOnTheExportedTimeline();
  void SaysWhyItCantStart();
};

void ExportTests::ExportsMp4AndGif() {
  QTemporaryDir dir;
  HistoryManager history(Lasting(12));
  PlaybackManager playback(&history);
  ExportManager exporter(&history, &playback);
  for (const VideoFormat format : {VideoFormat::kMp4, VideoFormat::kGif}) {
    const QString path = QDir(dir.path()).filePath(
        format == VideoFormat::kMp4 ? "out.mp4" : "out.gif");
    QSignalSpy finished(&exporter, &ExportManager::Finished);
    QSignalSpy progress(&exporter, &ExportManager::Progress);
    QVERIFY(exporter.Start({path, format, Frame(0), Frame(0), 0.5})
                .has_value());
    QVERIFY(finished.wait(10000));
    QCOMPARE(finished.first().first().toString(), path);
    QVERIFY(QFile(path).size() > 0);
    QCOMPARE(progress.last().at(0).toInt(), 12);
    QCOMPARE(progress.last().at(1).toInt(), 12);
  }
}

void ExportTests::CancellingLeavesNoFile() {
  QTemporaryDir dir;
  HistoryManager history(Lasting(24 * 60));
  PlaybackManager playback(&history);
  ExportManager exporter(&history, &playback);
  const QString path = QDir(dir.path()).filePath("long.mp4");
  QSignalSpy cancelled(&exporter, &ExportManager::Cancelled);
  QVERIFY(exporter.Start({path, VideoFormat::kMp4, Frame(0), Frame(0), 1.0})
              .has_value());
  QVERIFY(exporter.IsRunning());
  QCOMPARE(exporter.WhyNoExport(VideoFormat::kMp4),
           QString("An export is already running."));
  exporter.Cancel();
  QVERIFY(cancelled.wait(10000));
  QVERIFY(!QFile::exists(path));
}

void ExportTests::ExportsTheReelOnceItHasClips() {
  QTemporaryDir dir;
  HistoryManager history(Lasting(12));
  PlaybackManager playback(&history);
  ExportManager exporter(&history, &playback);
  QVERIFY(!exporter.IsReelExport());
  QCOMPARE(exporter.ExportLength(), Frame(12));
  history.Commit("Cut", LateOnTheReel(12));
  QVERIFY(exporter.IsReelExport());
  QCOMPARE(exporter.ExportLength(), Frame(18));
  const QString path = QDir(dir.path()).filePath("reel.mp4");
  QSignalSpy finished(&exporter, &ExportManager::Finished);
  QVERIFY(exporter.Start({path, VideoFormat::kMp4, Frame(0), Frame(0), 1.0})
              .has_value());
  QVERIFY(finished.wait(10000));
  VideoReader reader;
  QVERIFY(reader.Open(path).has_value());
  QVERIFY(std::abs(reader.info().seconds - 18.0 / kFramesPerSecond) < 0.05);
  const QColor before = reader.PictureAt(SecondsAtFrame(Frame(2)))
                            ->pixelColor(32, 24);
  const QColor during = reader.PictureAt(SecondsAtFrame(Frame(12)))
                            ->pixelColor(32, 24);
  QVERIFY(before.red() < 40 && before.blue() < 40);
  QVERIFY(during.red() > 200 && during.blue() > 200);
}

void ExportTests::LoopMustBeOnTheExportedTimeline() {
  HistoryManager history(LateOnTheReel(12));
  PlaybackManager playback(&history);
  ExportManager exporter(&history, &playback);
  QVERIFY(!exporter.WhyNoLoop().isEmpty());
  playback.SetLoop(Frame(1), Frame(4));
  QVERIFY(exporter.WhyNoLoop().contains("shots"));
  playback.SetTimeline(Timeline::kReel);
  playback.SetLoop(Frame(1), Frame(4));
  QVERIFY(exporter.WhyNoLoop().isEmpty());
}

void ExportTests::SaysWhyItCantStart() {
  QTemporaryDir dir;
  HistoryManager history{Project()};
  PlaybackManager playback(&history);
  ExportManager exporter(&history, &playback);
  QCOMPARE(exporter.WhyNoExport(VideoFormat::kGif),
           QString("Add a shot or a clip first."));
  history.Commit("Shot", Lasting(12));
  const QString path = QDir(dir.path()).filePath("x.gif");
  QVERIFY(!exporter.Start({path, VideoFormat::kGif, Frame(5), Frame(5), 1.0})
               .has_value());
  QVERIFY(!exporter.Start({"", VideoFormat::kGif, Frame(0), Frame(0), 1.0})
               .has_value());
  Project with_song = Lasting(12);
  with_song.song = QDir(dir.path()).filePath("missing.wav");
  history.Commit("Song", with_song);
  QTRY_VERIFY_WITH_TIMEOUT(!playback.IsLoadingSong(), 5000);
  QVERIFY(exporter.WhyNoExport(VideoFormat::kMp4).contains("can't be read"));
  QVERIFY(exporter.WhyNoExport(VideoFormat::kGif).isEmpty());
}

}  // namespace snapper

QTEST_MAIN(snapper::ExportTests)
#include "export_tests.moc"
