#include <QApplication>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QPushButton>
#include <QTest>

#include <cassert>

#include "anim/reel_timeline.h"
#include "bench.h"
#include "media/video_encoder.h"
#include "ui/reel_timeline.h"
#include "ui/shot_bin.h"
#include "ui/video_page.h"

namespace snapper {
namespace {

QPoint Middle(const QRectF& rect) { return rect.center().toPoint(); }

// The bench with one 48-frame shot, its playhead on the reel.
void AddShot(Bench* bench) {
  assert(bench != nullptr);
  QVERIFY(bench->shots.Add(-1).has_value());
  bench->playback.SetTimeline(Timeline::kReel);
}

// Drops a shot from the shot list at where on timeline.
void DropShot(ReelTimeline* timeline, ShotId shot, QPoint where) {
  assert(timeline != nullptr);
  assert(shot.IsValid());
  QMimeData data;
  data.setData(kShotMime, QByteArray::number(shot.value()));
  QDragEnterEvent enter(where, Qt::CopyAction, &data, Qt::LeftButton, {});
  QApplication::sendEvent(timeline, &enter);
  QDragMoveEvent move(where, Qt::CopyAction, &data, Qt::LeftButton, {});
  QApplication::sendEvent(timeline, &move);
  QDropEvent drop(where, Qt::CopyAction, &data, Qt::LeftButton, {});
  QApplication::sendEvent(timeline, &drop);
}

QString WriteTake(const QString& folder, const QString& name) {
  assert(!folder.isEmpty());
  const QString path = QDir(folder).filePath(name);
  VideoEncoder encoder;
  bool is_ok =
      encoder.Open({path, VideoFormat::kMp4, 32, 32, nullptr}).has_value();
  for (int i = 0; is_ok && i < kFramesPerSecond; ++i) {
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    is_ok = encoder.AddFrame(image).has_value();
  }
  const bool is_done = is_ok && encoder.Finish().has_value();
  // A finished take is on disk.
  assert(!is_done || QFile::exists(path));
  return is_done ? path : QString();
}

const Clip& OnlyClip(const Bench& bench, int track) {
  const auto& tracks = bench.history.current().reel.tracks;
  assert(track >= 0 && track < static_cast<int>(tracks.size()));
  const auto& clips = tracks[static_cast<size_t>(track)].clips;
  assert(clips.size() == 1);
  return clips.front();
}

}  // namespace

class VideoPageTests final : public QObject {
  Q_OBJECT

 private slots:
  void BinListsShotsToDrag();
  void DroppingAShotPlacesAClip();
  void DraggingAClipMovesIt();
  void DraggingAnEndTrims();
  void RulerSeeksTheReel();
  void KeysSplitAndRemove();
  void ImportedVideosQueueOnTheBottom();
  void MarkCutDropsAMarkerAtThePlayhead();
};

void VideoPageTests::BinListsShotsToDrag() {
  Bench bench;
  ShotBin bin(bench.All());
  bin.show();
  QCOMPARE(bin.count(), 0);
  AddShot(&bench);
  QVERIFY(bench.shots.Add(-1).has_value());
  QCOMPARE(bin.count(), 2);
  // Hidden, it catches up when shown again.
  bin.hide();
  QVERIFY(bench.shots.Add(-1).has_value());
  QCOMPARE(bin.count(), 2);
  bin.show();
  QCOMPARE(bin.count(), 3);
  QVERIFY(bin.item(0)->text().startsWith("Shot 1"));
  QVERIFY(!bin.item(0)->icon().isNull());
  QMimeData data;
  data.setData(kShotMime, "2");
  QCOMPARE(ShotOfDrop(&data), ShotId(2));
  QMimeData empty;
  QVERIFY(!ShotOfDrop(&empty).IsValid());
}

void VideoPageTests::DroppingAShotPlacesAClip() {
  Bench bench;
  AddShot(&bench);
  ReelTimeline timeline(bench.All());
  timeline.resize(1000, 200);
  const int x = static_cast<int>(timeline.XOf(Frame(30)));
  const int y = static_cast<int>(timeline.TrackTop(1)) + 10;
  QCOMPARE(timeline.TrackAt(y), 1);
  DropShot(&timeline, ShotId(1), QPoint(x, y));
  const Clip& clip = OnlyClip(bench, 1);
  QCOMPARE(clip.start, Frame(30));
  QCOMPARE(clip.length, Frame(48));
  QCOMPARE(bench.selection.clips(), std::set<ClipId>({clip.id}));
  // Dropping onto the ruler places nothing.
  DropShot(&timeline, ShotId(1), QPoint(x, 5));
  QCOMPARE(ReelLength(bench.history.current()), Frame(78));
}

void VideoPageTests::DraggingAClipMovesIt() {
  Bench bench;
  AddShot(&bench);
  const ClipId clip = *bench.reel.AddShot(ShotId(1), 0, Frame(0));
  ReelTimeline timeline(bench.All());
  timeline.resize(1000, 200);
  const QPoint from = Middle(timeline.ClipRect(clip));
  const int step = static_cast<int>(24 * timeline.zoom());
  const int up = static_cast<int>(timeline.TrackTop(0) -
                                  timeline.TrackTop(1));
  QTest::mousePress(&timeline, Qt::LeftButton, {}, from);
  QTest::mouseMove(&timeline, from + QPoint(step / 2, 0));
  QTest::mouseMove(&timeline, from + QPoint(step, -up));
  QTest::mouseRelease(&timeline, Qt::LeftButton, {},
                      from + QPoint(step, -up));
  QCOMPARE(FindClip(bench.history.current().reel, clip).track, 1);
  QCOMPARE(ClipOf(bench.history.current().reel, clip)->start, Frame(24));
  QCOMPARE(bench.history.UndoLabel(), QString("Move clip"));
  bench.history.Undo();
  QCOMPARE(ClipOf(bench.history.current().reel, clip)->start, Frame(0));
}

void VideoPageTests::DraggingAnEndTrims() {
  Bench bench;
  AddShot(&bench);
  const ClipId clip = *bench.reel.AddShot(ShotId(1), 0, Frame(100));
  ReelTimeline timeline(bench.All());
  timeline.resize(1000, 200);
  const QRectF box = timeline.ClipRect(clip);
  const QPoint edge(static_cast<int>(box.right()) - 2,
                    static_cast<int>(box.center().y()));
  QTest::mousePress(&timeline, Qt::LeftButton, {}, edge);
  const QPoint in(static_cast<int>(timeline.XOf(Frame(130))),
                  edge.y());
  QTest::mouseMove(&timeline, in);
  QTest::mouseRelease(&timeline, Qt::LeftButton, {}, in);
  QCOMPARE(ClipOf(bench.history.current().reel, clip)->length, Frame(30));
  QCOMPARE(bench.history.UndoLabel(), QString("Trim clip"));
}

void VideoPageTests::RulerSeeksTheReel() {
  Bench bench;
  AddShot(&bench);
  QVERIFY(bench.reel.AddShot(ShotId(1), 0, Frame(0)).has_value());
  ReelTimeline timeline(bench.All());
  timeline.resize(1000, 200);
  QTest::mouseClick(&timeline, Qt::LeftButton, {},
                    QPoint(static_cast<int>(timeline.XOf(Frame(20))) + 1, 5));
  QCOMPARE(bench.playback.frame(), Frame(20));
  QCOMPARE(bench.playback.FrameOn(Timeline::kShots), Frame(0));
}

void VideoPageTests::KeysSplitAndRemove() {
  Bench bench;
  AddShot(&bench);
  const ClipId clip = *bench.reel.AddShot(ShotId(1), 0, Frame(0));
  ReelTimeline timeline(bench.All());
  timeline.resize(1000, 200);
  bench.selection.PickClips({clip}, PickMode::kReplace);
  bench.playback.Seek(Frame(10));
  QTest::keyClick(&timeline, Qt::Key_S);
  QCOMPARE(bench.history.current().reel.tracks[0].clips.size(), size_t{2});
  QTest::keyClick(&timeline, Qt::Key_A, Qt::ControlModifier);
  QCOMPARE(bench.selection.clips().size(), size_t{2});
  QTest::keyClick(&timeline, Qt::Key_Delete);
  QVERIFY(IsReelEmpty(bench.history.current().reel));
}

void VideoPageTests::ImportedVideosQueueOnTheBottom() {
  Bench bench;
  VideoPage page(bench.All());
  const QString first = WriteTake(bench.dir.path(), "a.mp4");
  const QString second = WriteTake(bench.dir.path(), "b.mp4");
  page.ImportVideos({first, second});
  const auto& clips = bench.history.current().reel.tracks[0].clips;
  QCOMPARE(clips.size(), size_t{2});
  QCOMPARE(clips[1].start, clips[0].end());
  QCOMPARE(bench.selection.clips().size(), size_t{2});
}

void VideoPageTests::MarkCutDropsAMarkerAtThePlayhead() {
  Bench bench;
  AddShot(&bench);
  QVERIFY(bench.reel.AddShot(ShotId(1), 0, Frame(0)).has_value());
  VideoPage page(bench.All());
  auto* mark = page.findChild<QPushButton*>("mark_cut");
  QCOMPARE(mark->shortcut(), QKeySequence(Qt::Key_M));
  bench.playback.Seek(Frame(30));
  mark->click();
  QCOMPARE(bench.history.current().reel.markers,
           (std::vector<Frame>{Frame(30)}));
  mark->click();
  QVERIFY(bench.history.current().reel.markers.empty());
}

}  // namespace snapper

QTEST_MAIN(snapper::VideoPageTests)
#include "video_page_tests.moc"
