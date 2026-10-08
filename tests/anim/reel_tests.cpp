#include <QTest>

#include <memory>

#include "anim/reel_timeline.h"
#include "model/project.h"
#include "model/reel.h"

namespace snapper {
namespace {

Clip VideoClip(int id, int start, int length, int in = 0) {
  Clip clip;
  clip.id = ClipId(id);
  clip.source = VideoSource{"/takes/paint.mp4", Frame(1000)};
  clip.start = Frame(start);
  clip.length = Frame(length);
  clip.in = Frame(in);
  return clip;
}

Clip ShotClip(int id, ShotId shot, int start, int length) {
  Clip clip;
  clip.id = ClipId(id);
  clip.source = ShotSource{shot};
  clip.start = Frame(start);
  clip.length = Frame(length);
  return clip;
}

// A speedpaint on the bottom track and a 48-frame shot above it.
Project PaintWithShot() {
  Project project;
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(48);
  project.shots.push_back(std::make_shared<const Shot>(shot));
  PlaceClip(&project.reel.tracks[0], VideoClip(1, 0, 200, 10));
  PlaceClip(&project.reel.tracks[1], ShotClip(2, ShotId(1), 50, 48));
  return project;
}

}  // namespace

class ReelTests final : public QObject {
  Q_OBJECT

 private slots:
  void NewReelHasEmptyTracks();
  void ClipsStaySortedAndNeverOverlap();
  void FrameShowsEachTrackBottomFirst();
  void ShortenedShotLeavesTheTailEmpty();
  void GoneShotShowsNothing();
  void LengthAndCutsFollowClips();
  void DragsSnapToCutsAndThePlayhead();
};

void ReelTests::NewReelHasEmptyTracks() {
  const Project project;
  QCOMPARE(static_cast<int>(project.reel.tracks.size()), kDefaultReelTracks);
  QVERIFY(IsReelEmpty(project.reel));
  QCOMPARE(ReelLength(project), Frame(0));
  QVERIFY(ReelAt(project, Frame(0)).empty());
}

void ReelTests::ClipsStaySortedAndNeverOverlap() {
  ReelTrack track;
  PlaceClip(&track, VideoClip(1, 100, 50));
  PlaceClip(&track, VideoClip(2, 0, 50));
  QCOMPARE(track.clips[0].id, ClipId(2));
  QCOMPARE(track.clips[1].id, ClipId(1));
  QVERIFY(HasRoom(track, VideoClip(3, 50, 50)));
  QVERIFY(!HasRoom(track, VideoClip(3, 49, 2)));
  QVERIFY(!HasRoom(track, VideoClip(3, 120, 10)));
  // A clip never collides with itself, so it can be moved in place.
  QVERIFY(HasRoom(track, VideoClip(1, 90, 50)));
  Reel reel;
  reel.tracks[2] = track;
  QCOMPARE(FindClip(reel, ClipId(1)).track, 2);
  QCOMPARE(FindClip(reel, ClipId(1)).index, 1);
  QVERIFY(!FindClip(reel, ClipId(9)).IsValid());
  QVERIFY(ClipOf(reel, ClipId(9)) == nullptr);
}

void ReelTests::FrameShowsEachTrackBottomFirst() {
  const Project project = PaintWithShot();
  const auto before = ReelAt(project, Frame(20));
  QCOMPARE(static_cast<int>(before.size()), 1);
  QCOMPARE(before[0].track, 0);
  // The video shows from its in point.
  QCOMPARE(before[0].source, Frame(30));
  const auto during = ReelAt(project, Frame(60));
  QCOMPARE(static_cast<int>(during.size()), 2);
  QCOMPARE(during[0].track, 0);
  QCOMPARE(during[1].track, 1);
  QCOMPARE(during[1].clip->id, ClipId(2));
  QCOMPARE(during[1].source, Frame(10));
  QCOMPARE(static_cast<int>(ReelAt(project, Frame(98)).size()), 1);
}

void ReelTests::ShortenedShotLeavesTheTailEmpty() {
  Project project = PaintWithShot();
  Shot shorter = *project.shots[0];
  shorter.length = Frame(20);
  project.shots[0] = std::make_shared<const Shot>(shorter);
  const Clip& clip = project.reel.tracks[1].clips[0];
  QCOMPARE(ShownLength(project, clip), Frame(20));
  QCOMPARE(static_cast<int>(ReelAt(project, Frame(69)).size()), 2);
  QCOMPARE(static_cast<int>(ReelAt(project, Frame(70)).size()), 1);
  // The clip itself keeps its place and length.
  QCOMPARE(ReelLength(project), Frame(200));
}

void ReelTests::GoneShotShowsNothing() {
  Project project = PaintWithShot();
  project.shots.clear();
  const Clip& clip = project.reel.tracks[1].clips[0];
  QCOMPARE(SourceLength(project, clip), Frame(0));
  QCOMPARE(ShownLength(project, clip), Frame(0));
  QCOMPARE(static_cast<int>(ReelAt(project, Frame(60)).size()), 1);
}

void ReelTests::LengthAndCutsFollowClips() {
  Project project = PaintWithShot();
  PlaceClip(&project.reel.tracks[2], VideoClip(3, 250, 10));
  QCOMPARE(ReelLength(project), Frame(260));
  const std::set<Frame> cuts = ReelCuts(project);
  const std::set<Frame> expected = {Frame(0),   Frame(50),  Frame(98),
                                    Frame(200), Frame(250), Frame(260)};
  QCOMPARE(cuts, expected);
}

void ReelTests::DragsSnapToCutsAndThePlayhead() {
  const Project project = PaintWithShot();
  const std::set<ClipId> shot = {ClipId(2)};
  // The shot (50 to 98) dragged 99 frames: its start (149) is 3 from
  // nothing, its end (197) 3 from the speedpaint's end (200).
  QCOMPARE(SnapDelta(project, shot, 99, 4, Frame(500)), 102);
  QCOMPARE(SnapDelta(project, shot, 99, 2, Frame(500)), 99);
  // The playhead is a snap point too, and the nearest one wins.
  QCOMPARE(SnapDelta(project, shot, 99, 4, Frame(150)), 100);
  // A clip never snaps to its own cuts.
  QCOMPARE(SnapDelta(project, shot, 1, 4, Frame(500)), 1);
  QCOMPARE(SnapFrame(project, Frame(52), 3, {}, Frame(500)), Frame(50));
  QCOMPARE(SnapFrame(project, Frame(52), 3, shot, Frame(500)), Frame(52));
  QCOMPARE(SnapFrame(project, Frame(2), 3, shot, Frame(500)), Frame(0));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::ReelTests)
#include "reel_tests.moc"
