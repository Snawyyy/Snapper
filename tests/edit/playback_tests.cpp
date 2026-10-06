#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cstdint>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"

namespace snapper {
namespace {

// A project of one shot, length frames long.
Project Lasting(int length) {
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(length);
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

// A silent 16-bit mono WAV, seconds long.
QString WriteWav(const QString& folder, double seconds) {
  const QString path = QDir(folder).filePath("song.wav");
  const auto samples = static_cast<std::uint32_t>(seconds * 8000);
  QByteArray data(static_cast<qsizetype>(samples * 2), '\0');
  QByteArray wav;
  const auto put32 = [&wav](std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
      wav.append(static_cast<char>((v >> (8 * i)) & 0xff));
    }
  };
  const auto put16 = [&wav](std::uint16_t v) {
    wav.append(static_cast<char>(v & 0xff));
    wav.append(static_cast<char>(v >> 8));
  };
  wav.append("RIFF");
  put32(36 + static_cast<std::uint32_t>(data.size()));
  wav.append("WAVEfmt ");
  put32(16);
  put16(1);
  put16(1);
  put32(8000);
  put32(16000);
  put16(2);
  put16(16);
  wav.append("data");
  put32(static_cast<std::uint32_t>(data.size()));
  wav.append(data);
  QFile file(path);
  const bool is_written = file.open(QIODevice::WriteOnly) &&
                          file.write(wav) == wav.size();
  return is_written ? path : QString();
}

}  // namespace

class PlaybackTests final : public QObject {
  Q_OBJECT

 private slots:
  void SeekAndStepStayOnTheTrack();
  void AnimationModeSkipsHeldFrames();
  void PlayingMovesTheFrame();
  void PlayingStopsAtTheEnd();
  void LoopsWrapAround();
  void SongsLoadInTheBackground();
};

void PlaybackTests::SeekAndStepStayOnTheTrack() {
  HistoryManager history(Lasting(48));
  PlaybackManager playback(&history);
  QSignalSpy moved(&playback, &PlaybackManager::FrameChanged);
  playback.Seek(Frame(100));
  QCOMPARE(playback.frame(), Frame(47));
  playback.Step(-2);
  QCOMPARE(playback.frame(), Frame(45));
  playback.Step(-100);
  QCOMPARE(playback.frame(), Frame(0));
  QCOMPARE(moved.count(), 3);
  history.Commit("Shorter", Lasting(10));
  playback.Seek(Frame(9));
  history.Commit("Shortest", Lasting(4));
  QCOMPARE(playback.frame(), Frame(3));
}

void PlaybackTests::PlayingMovesTheFrame() {
  HistoryManager history(Lasting(480));
  PlaybackManager playback(&history);
  playback.Play();
  QVERIFY(playback.IsPlaying());
  QTRY_VERIFY_WITH_TIMEOUT(playback.frame() >= Frame(2), 2000);
  playback.Toggle();
  QVERIFY(!playback.IsPlaying());
  const Frame stopped = playback.frame();
  QTest::qWait(100);
  QCOMPARE(playback.frame(), stopped);
}

void PlaybackTests::PlayingStopsAtTheEnd() {
  HistoryManager history(Lasting(3));
  PlaybackManager playback(&history);
  QSignalSpy playing(&playback, &PlaybackManager::PlayingChanged);
  playback.Play();
  QTRY_VERIFY_WITH_TIMEOUT(!playback.IsPlaying(), 2000);
  QCOMPARE(playback.frame(), Frame(2));
  QCOMPARE(playing.count(), 2);
}

void PlaybackTests::LoopsWrapAround() {
  HistoryManager history(Lasting(480));
  PlaybackManager playback(&history);
  playback.SetLoop(Frame(10), Frame(12));
  playback.Seek(Frame(10));
  QSignalSpy moved(&playback, &PlaybackManager::FrameChanged);
  playback.Play();
  QTest::qWait(300);
  QVERIFY(playback.IsPlaying());
  QVERIFY(playback.frame() >= Frame(10) && playback.frame() < Frame(12));
  QVERIFY(moved.count() >= 3);
  playback.Pause();
  playback.ClearLoop();
  QVERIFY(!playback.HasLoop());
}

void PlaybackTests::SongsLoadInTheBackground() {
  QTemporaryDir dir;
  Project project = Lasting(48);
  project.song = WriteWav(dir.path(), 2.0);
  QVERIFY(!project.song.isEmpty());
  HistoryManager history(Project{});
  PlaybackManager playback(&history);
  QSignalSpy changed(&playback, &PlaybackManager::SongChanged);
  history.Commit("Song", project);
  QTRY_VERIFY_WITH_TIMEOUT(playback.song() != nullptr, 5000);
  QVERIFY(std::abs(playback.song()->Seconds() - 2.0) < 0.05);
  QVERIFY(playback.song_error().isEmpty());
  project.song = QDir(dir.path()).filePath("gone.wav");
  history.Commit("Bad song", project);
  QTRY_VERIFY_WITH_TIMEOUT(!playback.song_error().isEmpty(), 5000);
  QVERIFY(playback.song() == nullptr);
}

void PlaybackTests::AnimationModeSkipsHeldFrames() {
  // Camera keys at 4 (held) and 10 (easing into 13).
  Project project = Lasting(48);
  Shot shot = *project.shots[0];
  SetKey(&shot.camera, {Frame(4), CameraPose(), Ease::kStep});
  SetKey(&shot.camera, {Frame(10), CameraPose(), Ease::kLinear});
  SetKey(&shot.camera, {Frame(13), CameraPose(), Ease::kStep});
  project.shots[0] = std::make_shared<const Shot>(shot);
  HistoryManager history(project);
  PlaybackManager playback(&history);
  QSignalSpy changed(&playback, &PlaybackManager::StepModeChanged);
  playback.SetStepMode(StepMode::kAnimation);
  QCOMPARE(changed.count(), 1);
  playback.Step(1);
  QCOMPARE(playback.frame(), Frame(4));
  playback.Step(1);
  QCOMPARE(playback.frame(), Frame(10));
  // Through an ease every frame changes.
  playback.Step(1);
  QCOMPARE(playback.frame(), Frame(11));
  playback.Step(2);
  QCOMPARE(playback.frame(), Frame(13));
  playback.Step(-2);
  QCOMPARE(playback.frame(), Frame(11));
  // Past the last change, the end; before the first, the start.
  playback.Step(5);
  QCOMPARE(playback.frame(), Frame(47));
  playback.Step(-10);
  QCOMPARE(playback.frame(), Frame(0));
  playback.SetStepMode(StepMode::kScrub);
  playback.Step(1);
  QCOMPARE(playback.frame(), Frame(1));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::PlaybackTests)
#include "playback_tests.moc"
