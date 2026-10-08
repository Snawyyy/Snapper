#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <set>
#include <variant>

#include "anim/reel_timeline.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"
#include "media/video_encoder.h"

namespace snapper {
namespace {

// One 48-frame shot, as a new project has after adding a shot.
struct Fixture final {
  Fixture() : history(Project()), shots(&history), reel(&history) {
    shot = *shots.Add(-1);
  }
  const Clip& ClipAt(int track, int index) const {
    return history.current()
        .reel.tracks[static_cast<size_t>(track)]
        .clips[static_cast<size_t>(index)];
  }
  HistoryManager history;
  ShotManager shots;
  ReelManager reel;
  ShotId shot;
};

// A one-second video file.
QString WriteTake(const QTemporaryDir& dir) {
  const QString path = QDir(dir.path()).filePath("paint.mp4");
  VideoEncoder encoder;
  const bool is_open =
      encoder.Open({path, VideoFormat::kMp4, 32, 32, nullptr}).has_value();
  for (int i = 0; is_open && i < kFramesPerSecond; ++i) {
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::darkGreen);
    encoder.AddFrame(image);
  }
  const bool is_done = is_open && encoder.Finish().has_value();
  return is_done ? path : QString();
}

}  // namespace

class ReelManagerTests final : public QObject {
  Q_OBJECT

 private slots:
  void ShotsAndVideosGoOnWhole();
  void ClipsNeverLandOnEachOther();
  void DragMovesFromWhereItStarted();
  void MovesStayOnTheReel();
  void TrimsStopAtTheSource();
  void SplitCutsInTwo();
  void RemovedClipsLeaveThePick();
  void TracksComeAndGo();
};

void ReelManagerTests::ShotsAndVideosGoOnWhole() {
  Fixture f;
  const auto clip = f.reel.AddShot(f.shot, 1, Frame(10));
  QVERIFY(clip.has_value());
  QCOMPARE(f.ClipAt(1, 0).length, Frame(48));
  QCOMPARE(f.ClipAt(1, 0).start, Frame(10));
  QCOMPARE(f.history.UndoLabel(), QString("Add shot to video"));
  QTemporaryDir dir;
  const QString path = WriteTake(dir);
  const auto video = f.reel.AddVideo(path, 0, Frame(0));
  QVERIFY(video.has_value());
  QVERIFY(*video != *clip);
  const Clip& paint = f.ClipAt(0, 0);
  QVERIFY(std::abs(paint.length.index() - kFramesPerSecond) <= 1);
  QCOMPARE(std::get<VideoSource>(paint.source).path, path);
  QVERIFY(!f.reel.AddVideo(QDir(dir.path()).filePath("none.mp4"), 0,
                           Frame(100))
               .has_value());
  QVERIFY(!f.reel.AddShot(ShotId(99), 0, Frame(100)).has_value());
  QVERIFY(!f.reel.AddShot(f.shot, 7, Frame(100)).has_value());
  f.history.Undo();
  QCOMPARE(f.history.current().reel.tracks[0].clips.size(), size_t{0});
}

void ReelManagerTests::ClipsNeverLandOnEachOther() {
  Fixture f;
  QVERIFY(f.reel.AddShot(f.shot, 0, Frame(0)).has_value());
  const auto clash = f.reel.AddShot(f.shot, 0, Frame(47));
  QVERIFY(!clash.has_value());
  QVERIFY(clash.error().message.contains("in the way"));
  QVERIFY(f.reel.AddShot(f.shot, 0, Frame(48)).has_value());
  QCOMPARE(f.history.current().reel.tracks[0].clips.size(), size_t{2});
}

void ReelManagerTests::DragMovesFromWhereItStarted() {
  Fixture f;
  const ClipId moving = *f.reel.AddShot(f.shot, 0, Frame(0));
  const ClipId other = *f.reel.AddShot(f.shot, 0, Frame(200));
  {
    EditScope drag(&f.history, "Move clip");
    QVERIFY(f.reel.MoveAll({moving}, 0, 10).has_value());
    QVERIFY(f.reel.MoveAll({moving}, 0, 30).has_value());
    // Onto the other clip: refused, the last good spot stays.
    QVERIFY(!f.reel.MoveAll({moving}, 0, 180).has_value());
    QCOMPARE(ClipOf(f.history.current().reel, moving)->start, Frame(30));
  }
  QCOMPARE(ClipOf(f.history.current().reel, moving)->start, Frame(30));
  QCOMPARE(f.history.UndoLabel(), QString("Move clip"));
  f.history.Undo();
  QCOMPARE(ClipOf(f.history.current().reel, moving)->start, Frame(0));
  // Both together keep their gap.
  QVERIFY(f.reel.MoveAll({moving, other}, 1, 5).has_value());
  QCOMPARE(FindClip(f.history.current().reel, other).track, 1);
  QCOMPARE(ClipOf(f.history.current().reel, other)->start, Frame(205));
  QCOMPARE(f.history.UndoLabel(), QString("Move clips"));
}

void ReelManagerTests::MovesStayOnTheReel() {
  Fixture f;
  const ClipId clip = *f.reel.AddShot(f.shot, 0, Frame(5));
  QVERIFY(!f.reel.MoveAll({clip}, 0, -6).has_value());
  QVERIFY(!f.reel.MoveAll({clip}, -1, 0).has_value());
  QVERIFY(!f.reel.MoveAll({clip}, kDefaultReelTracks, 0).has_value());
  QVERIFY(!f.reel.MoveAll({ClipId(99)}, 0, 1).has_value());
  QVERIFY(f.reel.MoveAll({clip}, 2, -5).has_value());
  QCOMPARE(FindClip(f.history.current().reel, clip).track, 2);
  QCOMPARE(ClipOf(f.history.current().reel, clip)->start, Frame(0));
}

void ReelManagerTests::TrimsStopAtTheSource() {
  Fixture f;
  const ClipId clip = *f.reel.AddShot(f.shot, 0, Frame(100));
  QVERIFY(f.reel.TrimStart(clip, Frame(110)).has_value());
  const Clip* trimmed = ClipOf(f.history.current().reel, clip);
  QCOMPARE(trimmed->in, Frame(10));
  QCOMPARE(trimmed->length, Frame(38));
  QCOMPARE(trimmed->end(), Frame(148));
  // The front can come back only as far as the shot's first frame.
  QVERIFY(!f.reel.TrimStart(clip, Frame(99)).has_value());
  QVERIFY(f.reel.TrimStart(clip, Frame(100)).has_value());
  QVERIFY(!f.reel.TrimEnd(clip, Frame(149)).has_value());
  QVERIFY(f.reel.TrimEnd(clip, Frame(120)).has_value());
  QCOMPARE(ClipOf(f.history.current().reel, clip)->length, Frame(20));
  QVERIFY(!f.reel.TrimEnd(clip, Frame(100)).has_value());
  // A shot shortened later can still be trimmed shorter on the reel.
  QVERIFY(f.shots.SetLength(f.shot, Frame(10)).has_value());
  QVERIFY(f.reel.TrimEnd(clip, Frame(115)).has_value());
  QCOMPARE(ShownLength(f.history.current(),
                       *ClipOf(f.history.current().reel, clip)),
           Frame(10));
}

void ReelManagerTests::SplitCutsInTwo() {
  Fixture f;
  const ClipId clip = *f.reel.AddShot(f.shot, 0, Frame(10));
  QVERIFY(!f.reel.Split(clip, Frame(10)).has_value());
  QVERIFY(!f.reel.Split(clip, Frame(58)).has_value());
  const auto right = f.reel.Split(clip, Frame(30));
  QVERIFY(right.has_value());
  QCOMPARE(f.ClipAt(0, 0).length, Frame(20));
  QCOMPARE(f.ClipAt(0, 1).id, *right);
  QCOMPARE(f.ClipAt(0, 1).start, Frame(30));
  QCOMPARE(f.ClipAt(0, 1).in, Frame(20));
  QCOMPARE(f.ClipAt(0, 1).length, Frame(28));
  QCOMPARE(f.history.UndoLabel(), QString("Split clip"));
  f.history.Undo();
  QCOMPARE(f.history.current().reel.tracks[0].clips.size(), size_t{1});
}

void ReelManagerTests::RemovedClipsLeaveThePick() {
  Fixture f;
  SelectionManager selection(&f.history);
  const ClipId first = *f.reel.AddShot(f.shot, 0, Frame(0));
  const ClipId second = *f.reel.AddShot(f.shot, 1, Frame(0));
  selection.PickClips({first}, PickMode::kReplace);
  selection.PickClips({second, ClipId(99)}, PickMode::kAdd);
  QCOMPARE(selection.clips(), std::set<ClipId>({first, second}));
  QVERIFY(f.reel.RemoveAll({first}).has_value());
  QCOMPARE(selection.clips(), std::set<ClipId>({second}));
  QVERIFY(!f.reel.RemoveAll({}).has_value());
  QCOMPARE(f.history.UndoLabel(), QString("Remove clip"));
}

void ReelManagerTests::TracksComeAndGo() {
  Fixture f;
  QVERIFY(f.reel.AddTrack().has_value());
  QCOMPARE(f.history.current().reel.tracks.size(),
           size_t{kDefaultReelTracks + 1});
  QVERIFY(f.reel.AddShot(f.shot, 0, Frame(0)).has_value());
  QVERIFY(!f.reel.RemoveTrack(0).has_value());
  QVERIFY(f.reel.RemoveTrack(1).has_value());
  for (int i = 0; i < kMaxReelTracks; ++i) {
    f.reel.AddTrack();
  }
  QCOMPARE(f.history.current().reel.tracks.size(), size_t{kMaxReelTracks});
  QVERIFY(!f.reel.AddTrack().has_value());
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::ReelManagerTests)
#include "reel_manager_tests.moc"
