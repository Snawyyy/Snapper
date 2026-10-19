#include <QTest>

#include <cassert>

#include "model/character.h"
#include "model/key.h"
#include "model/layer.h"
#include "model/project.h"
#include "model/reel.h"
#include "model/shot.h"
#include "model/transition.h"

namespace snapper {
class ModelTests final : public QObject {
  Q_OBJECT

 private slots:
  void DefaultProjectIsFullHd();
  void CanvasMustBeEvenAndInRange();
  void KeysStaySortedAndUnique();
  void RemovingAMissingKeySaysSo();
  void LayersShowOnlyInTheirRange();
  void TransitionNeverEatsAWholeClip();
  void ClipsTouchWhereOneEndsAndTheNextStarts();
  void ProjectFindsShotsAndCharacters();
  void LinksToGoneLayersDrop();
};

void ModelTests::DefaultProjectIsFullHd() {
  const Project project;
  QCOMPARE(project.canvas, (CanvasSize{1920, 1080}));
  QVERIFY(IsValidCanvas(project.canvas));
  QVERIFY(!project.name.isEmpty());
}

void ModelTests::CanvasMustBeEvenAndInRange() {
  QVERIFY(IsValidCanvas({1080, 1920}));
  QVERIFY(IsValidCanvas({kMinCanvasSide, kMaxCanvasSide}));
  QVERIFY(!IsValidCanvas({1921, 1080}));
  QVERIFY(!IsValidCanvas({kMinCanvasSide - 2, 1080}));
  QVERIFY(!IsValidCanvas({1920, kMaxCanvasSide + 2}));
  QVERIFY(!IsValidCanvas({0, 0}));
}

void ModelTests::KeysStaySortedAndUnique() {
  Channel<double> channel;
  QVERIFY(SetKey(&channel, {Frame(10), 1.0, Ease::kStep}));
  QVERIFY(SetKey(&channel, {Frame(2), 2.0, Ease::kLinear}));
  QVERIFY(SetKey(&channel, {Frame(10), 3.0, Ease::kStep}));
  QCOMPARE(channel.keys.size(), size_t{2});
  QVERIFY(IsSorted(channel));
  QCOMPARE(channel.keys[1].value, 3.0);
  QCOMPARE(KeyIndexAt(channel, Frame(2)), 0);
  QCOMPARE(KeyIndexAt(channel, Frame(3)), -1);
}

void ModelTests::RemovingAMissingKeySaysSo() {
  Channel<int> channel;
  QVERIFY(SetKey(&channel, {Frame(4), 1, Ease::kStep}));
  QVERIFY(!RemoveKey(&channel, Frame(5)));
  QVERIFY(RemoveKey(&channel, Frame(4)));
  QVERIFY(channel.keys.empty());
}

void ModelTests::LayersShowOnlyInTheirRange() {
  Layer layer;
  layer.start = Frame(5);
  layer.length = Frame(3);
  QVERIFY(!IsLayerLive(layer, Frame(4), Frame(20)));
  QVERIFY(IsLayerLive(layer, Frame(7), Frame(20)));
  QVERIFY(!IsLayerLive(layer, Frame(8), Frame(20)));
  layer.length = Frame(0);
  QVERIFY(IsLayerLive(layer, Frame(19), Frame(20)));
  QVERIFY(!IsLayerLive(layer, Frame(20), Frame(20)));
  layer.is_visible = false;
  QVERIFY(!IsLayerLive(layer, Frame(7), Frame(20)));
}

void ModelTests::TransitionNeverEatsAWholeClip() {
  const Transition swipe{TransitionKind::kSwipeLeft, Frame(30)};
  QCOMPARE(UsableLength(swipe, Frame(10), Frame(4)), Frame(3));
  QCOMPARE(UsableLength(swipe, Frame(100), Frame(100)), Frame(30));
  QCOMPARE(UsableLength(swipe, Frame(1), Frame(100)), Frame(0));
  const Transition cut{TransitionKind::kCut, Frame(6)};
  QCOMPARE(UsableLength(cut, Frame(100), Frame(100)), Frame(0));
}

void ModelTests::ClipsTouchWhereOneEndsAndTheNextStarts() {
  ReelTrack track;
  Clip a;
  a.id = ClipId(1);
  a.length = Frame(10);
  Clip b = a;
  b.id = ClipId(2);
  b.start = Frame(10);
  Clip c = a;
  c.id = ClipId(3);
  c.start = Frame(25);
  PlaceClip(&track, a);
  PlaceClip(&track, c);
  PlaceClip(&track, b);
  QCOMPARE(NextTouching(track, a)->id, ClipId(2));
  QVERIFY(NextTouching(track, b) == nullptr);
  QVERIFY(NextTouching(track, c) == nullptr);
}

void ModelTests::ProjectFindsShotsAndCharacters() {
  Project project;
  Shot shot;
  shot.id = ShotId(7);
  Layer layer;
  layer.id = LayerId(3);
  shot.layers.push_back(layer);
  project.shots.push_back(std::make_shared<const Shot>(shot));
  Character bob;
  bob.name = "Bob";
  project.characters["Bob"] = std::make_shared<const Character>(bob);
  QCOMPARE(ShotIndex(project, ShotId(7)), 0);
  QCOMPARE(ShotIndex(project, ShotId(8)), -1);
  QVERIFY(FindLayer(*FindShot(project, ShotId(7)), LayerId(3)) != nullptr);
  QVERIFY(FindCharacter(project, "Bob") != nullptr);
  QVERIFY(FindCharacter(project, "Ann") == nullptr);
}

void ModelTests::LinksToGoneLayersDrop() {
  Shot shot;
  Layer a;
  a.id = LayerId(1);
  Layer b;
  b.id = LayerId(2);
  shot.layers = {a, b};
  shot.links = {Link{{LayerId(1)}, {LayerId(2)}, Frame(0), 1.0},
                Link{{LayerId(2)}, {LayerId(3)}, Frame(0), 1.0}};
  QVERIFY(FindLink(shot, {LayerId(1)}) != nullptr);
  QVERIFY(FindLink(shot, {LayerId(3)}) == nullptr);
  DropDeadLinks(&shot);
  QCOMPARE(shot.links.size(), size_t{1});
  shot.layers.pop_back();
  DropDeadLinks(&shot);
  QVERIFY(shot.links.empty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ModelTests)
#include "model_tests.moc"
