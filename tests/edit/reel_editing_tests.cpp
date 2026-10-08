#include <QTest>

#include <cassert>
#include <set>
#include <vector>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "edit/shot_manager.h"

namespace snapper {
namespace {

// One 48-frame shot, as a new project has after adding a shot.
struct Fixture final {
  Fixture() : history(Project()), shots(&history), reel(&history) {
    shot = *shots.Add(-1);
  }
  const std::vector<Clip>& Track(int track) const {
    assert(track >= 0);
    return history.current().reel.tracks[static_cast<size_t>(track)].clips;
  }
  const Clip& Of(ClipId id) const {
    const Clip* clip = ClipOf(history.current().reel, id);
    assert(clip != nullptr);
    return *clip;
  }
  HistoryManager history;
  ShotManager shots;
  ReelManager reel;
  ShotId shot;
};

}  // namespace

class ReelEditingTests final : public QObject {
  Q_OBJECT

 private slots:
  void CopyPasteKeepsTracksAndSpacing();
  void PasteIsRefusedOverAClip();
  void CutTakesTheClipsAway();
  void DuplicateLaysCopiesAfter();
  void SplitWithoutAPickCutsUnderThePlayhead();
  void RippleRemoveClosesTheGap();
  void BoxPicksWhatItTouches();
};

void ReelEditingTests::CopyPasteKeepsTracksAndSpacing() {
  Fixture f;
  const ClipId low = *f.reel.AddShot(f.shot, 0, Frame(10));
  const ClipId high = *f.reel.AddShot(f.shot, 1, Frame(30));
  QVERIFY(f.reel.TrimStart(low, Frame(14)).has_value());
  QVERIFY(!f.reel.WhyNoPaste(Frame(200)).isEmpty());
  QVERIFY(!f.reel.Copy({}).has_value());
  QVERIFY(f.reel.Copy({high, low}).has_value());
  const QString before = f.history.UndoLabel();
  QCOMPARE(before, QString("Trim clip"));
  QVERIFY(f.reel.WhyNoPaste(Frame(200)).isEmpty());
  const auto pasted = f.reel.Paste(Frame(200));
  QVERIFY(pasted.has_value());
  QCOMPARE(pasted->size(), size_t{2});
  QCOMPARE(f.history.UndoLabel(), QString("Paste clips"));
  // The first copy starts at the playhead, the other keeps its gap,
  // each on its own track, showing the same part of its source.
  QCOMPARE(f.Of((*pasted)[0]).start, Frame(200));
  QCOMPARE(f.Of((*pasted)[0]).in, Frame(4));
  QCOMPARE(FindClip(f.history.current().reel, (*pasted)[0]).track, 0);
  QCOMPARE(f.Of((*pasted)[1]).start, Frame(216));
  QCOMPARE(FindClip(f.history.current().reel, (*pasted)[1]).track, 1);
  QVERIFY((*pasted)[0] != low && (*pasted)[1] != high);
  f.history.Undo();
  QCOMPARE(f.Track(0).size(), size_t{1});
}

void ReelEditingTests::PasteIsRefusedOverAClip() {
  Fixture f;
  const ClipId clip = *f.reel.AddShot(f.shot, 0, Frame(0));
  QVERIFY(f.reel.Copy({clip}).has_value());
  QVERIFY(f.reel.WhyNoPaste(Frame(20)).contains("in the way"));
  QVERIFY(!f.reel.Paste(Frame(20)).has_value());
  QCOMPARE(f.Track(0).size(), size_t{1});
  QVERIFY(f.reel.Paste(Frame(48)).has_value());
  QCOMPARE(f.history.UndoLabel(), QString("Paste clip"));
}

void ReelEditingTests::CutTakesTheClipsAway() {
  Fixture f;
  const ClipId clip = *f.reel.AddShot(f.shot, 2, Frame(5));
  QVERIFY(!f.reel.WhyNoPick({}).isEmpty());
  QVERIFY(f.reel.WhyNoPick({clip}).isEmpty());
  QVERIFY(f.reel.CutAll({clip}).has_value());
  QVERIFY(IsReelEmpty(f.history.current().reel));
  QCOMPARE(f.history.UndoLabel(), QString("Cut clip"));
  const auto pasted = f.reel.Paste(Frame(0));
  QVERIFY(pasted.has_value());
  QCOMPARE(f.Track(2).size(), size_t{1});
  QCOMPARE(f.Track(2)[0].length, Frame(48));
}

void ReelEditingTests::DuplicateLaysCopiesAfter() {
  Fixture f;
  const ClipId a = *f.reel.AddShot(f.shot, 0, Frame(0));
  const ClipId b = *f.reel.AddShot(f.shot, 1, Frame(20));
  QVERIFY(!f.reel.WhyNoDuplicate({}).isEmpty());
  const auto copies = f.reel.DuplicateAll({a, b});
  QVERIFY(copies.has_value());
  QCOMPARE(f.history.UndoLabel(), QString("Duplicate clips"));
  // The block (0 to 68) is laid again from where it ends.
  QCOMPARE(f.Of((*copies)[0]).start, Frame(68));
  QCOMPARE(f.Of((*copies)[1]).start, Frame(88));
  QCOMPARE(f.Track(0).size(), size_t{2});
  // A clip right after leaves no room.
  QVERIFY(f.reel.AddShot(f.shot, 0, Frame(200)).has_value());
  const ClipId last = f.Track(0).back().id;
  QVERIFY(f.reel.MoveAll({last}, 0, -84).has_value());
  QVERIFY(!f.reel.WhyNoDuplicate({(*copies)[0]}).isEmpty());
}

void ReelEditingTests::SplitWithoutAPickCutsUnderThePlayhead() {
  Fixture f;
  const ClipId a = *f.reel.AddShot(f.shot, 0, Frame(0));
  const ClipId b = *f.reel.AddShot(f.shot, 1, Frame(10));
  const ClipId c = *f.reel.AddShot(f.shot, 2, Frame(30));
  QCOMPARE(f.reel.SplitTargets({}, Frame(20)),
           (std::vector<ClipId>{a, b}));
  QCOMPARE(f.reel.SplitTargets({c}, Frame(20)), (std::vector<ClipId>{c}));
  const auto halves = f.reel.SplitAll({}, Frame(20));
  QVERIFY(halves.has_value());
  QCOMPARE(halves->size(), size_t{2});
  QCOMPARE(f.history.UndoLabel(), QString("Split clips"));
  QCOMPARE(f.Of(a).length, Frame(20));
  QCOMPARE(f.Of(b).length, Frame(10));
  QCOMPARE(f.Track(2).size(), size_t{1});
  QVERIFY(f.reel.WhyNoSplit({}, Frame(400)).contains("inside a clip"));
}

void ReelEditingTests::RippleRemoveClosesTheGap() {
  Fixture f;
  const ClipId a = *f.reel.AddShot(f.shot, 0, Frame(0));
  const ClipId b = *f.reel.AddShot(f.shot, 0, Frame(48));
  const ClipId c = *f.reel.AddShot(f.shot, 0, Frame(100));
  const ClipId other = *f.reel.AddShot(f.shot, 1, Frame(100));
  QVERIFY(!f.reel.RippleDeleteAll({}).has_value());
  QVERIFY(f.reel.RippleDeleteAll({a}).has_value());
  QCOMPARE(f.history.UndoLabel(), QString("Ripple remove clip"));
  QCOMPARE(f.Of(b).start, Frame(0));
  QCOMPARE(f.Of(c).start, Frame(52));
  // Other tracks stay where they are.
  QCOMPARE(f.Of(other).start, Frame(100));
  QVERIFY(f.reel.RippleDeleteAll({b, other}).has_value());
  QCOMPARE(f.Of(c).start, Frame(4));
  QCOMPARE(f.history.UndoLabel(), QString("Ripple remove clips"));
  f.history.Undo();
  f.history.Undo();
  QCOMPARE(f.Of(a).start, Frame(0));
  QCOMPARE(f.Of(c).start, Frame(100));
}

void ReelEditingTests::BoxPicksWhatItTouches() {
  Fixture f;
  SelectionManager selection(&f.history);
  const ClipId a = *f.reel.AddShot(f.shot, 0, Frame(0));
  const ClipId b = *f.reel.AddShot(f.shot, 1, Frame(60));
  const ClipId c = *f.reel.AddShot(f.shot, 2, Frame(0));
  const Reel& reel = f.history.current().reel;
  QCOMPARE(AllClips(reel), (std::set<ClipId>{a, b, c}));
  // Tracks 0 to 1, frames 40 to 70: touches a's tail and b's head.
  QCOMPARE(ClipsIn(reel, 1, 0, Frame(70), Frame(40)),
           (std::set<ClipId>{a, b}));
  QCOMPARE(ClipsIn(reel, 0, 2, Frame(48), Frame(59)), std::set<ClipId>());
  QCOMPARE(ClipsIn(reel, -1, -1, Frame(0), Frame(9)), std::set<ClipId>());
  selection.PickClips(ClipsIn(reel, 0, 0, Frame(0), Frame(5)),
                      PickMode::kReplace);
  selection.PickClips(ClipsIn(reel, 2, 2, Frame(0), Frame(5)),
                      PickMode::kAdd);
  QCOMPARE(selection.clips(), (std::set<ClipId>{a, c}));
  selection.PickClips({a}, PickMode::kRemove);
  QCOMPARE(selection.clips(), (std::set<ClipId>{c}));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::ReelEditingTests)
#include "reel_editing_tests.moc"
