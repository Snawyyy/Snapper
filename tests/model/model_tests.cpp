#include <QTest>

#include "model/doll.h"
#include "model/key.h"
#include "model/layer.h"
#include "model/project.h"
#include "model/shot.h"

namespace snapper {
namespace {

Doll Arm() {
  Doll doll;
  doll.name = "Bob";
  doll.folder = "/dolls/Bob.doll";
  doll.art.pieces = {{"body", {"body.png"}, 0, {}, {10, 10}},
                     {"arm", {"open.png", "fist.png"}, 0, {}, {4, 4}},
                     {"hand", {"hand.png"}, 0, {}, {2, 2}}};
  doll.rig.pieces = {{"body", "", {}, 0, -1, {}},
                     {"arm", "body", {}, 1, -1, {}},
                     {"hand", "arm", {}, 2, -1, {}}};
  return doll;
}

}  // namespace

class ModelTests final : public QObject {
  Q_OBJECT

 private slots:
  void DefaultProjectIsFullHd();
  void CanvasMustBeEvenAndInRange();
  void KeysStaySortedAndUnique();
  void RemovingAMissingKeySaysSo();
  void ParentingRefusesLoops();
  void DrawingFallsBackToRigThenArtDefault();
  void LayersShowOnlyInTheirRange();
  void TransitionNeverEatsAWholeShot();
  void ProjectFindsShotsAndDolls();
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

void ModelTests::ParentingRefusesLoops() {
  const Doll doll = Arm();
  QVERIFY(CanParent(doll.rig, "hand", "body"));
  QVERIFY(CanParent(doll.rig, "arm", ""));
  QVERIFY(!CanParent(doll.rig, "body", "hand"));
  QVERIFY(!CanParent(doll.rig, "arm", "arm"));
  QVERIFY(!CanParent(doll.rig, "arm", "ghost"));
  QVERIFY(!CanParent(doll.rig, "ghost", ""));
}

void ModelTests::DrawingFallsBackToRigThenArtDefault() {
  Doll doll = Arm();
  QCOMPARE(DrawingPath(doll, "arm", -1), QString("/dolls/Bob.doll/open.png"));
  FindRig(&doll.rig, "arm")->default_drawing = 1;
  QCOMPARE(DrawingPath(doll, "arm", -1), QString("/dolls/Bob.doll/fist.png"));
  QCOMPARE(DrawingPath(doll, "arm", 0), QString("/dolls/Bob.doll/open.png"));
  QVERIFY(DrawingPath(doll, "arm", 2).isEmpty());
  QVERIFY(DrawingPath(doll, "ghost", 0).isEmpty());
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

void ModelTests::TransitionNeverEatsAWholeShot() {
  Shot a;
  a.length = Frame(10);
  a.transition = {TransitionKind::kSwipeLeft, Frame(30)};
  Shot b;
  b.length = Frame(4);
  QCOMPARE(UsableTransition(a, &b), Frame(3));
  QCOMPARE(UsableTransition(a, nullptr), Frame(0));
  a.transition.kind = TransitionKind::kCut;
  QCOMPARE(UsableTransition(a, &b), Frame(0));
}

void ModelTests::ProjectFindsShotsAndDolls() {
  Project project;
  Shot shot;
  shot.id = ShotId(7);
  Layer layer;
  layer.id = LayerId(3);
  shot.layers.push_back(layer);
  project.shots.push_back(std::make_shared<const Shot>(shot));
  project.dolls["Bob"] = std::make_shared<const Doll>(Arm());
  QCOMPARE(ShotIndex(project, ShotId(7)), 0);
  QCOMPARE(ShotIndex(project, ShotId(8)), -1);
  QVERIFY(FindLayer(*FindShot(project, ShotId(7)), LayerId(3)) != nullptr);
  QVERIFY(FindDoll(project, "Bob") != nullptr);
  QVERIFY(FindDoll(project, "Ann") == nullptr);
}

void ModelTests::LinksToGoneLayersDrop() {
  Shot shot;
  Layer a;
  a.id = LayerId(1);
  Layer b;
  b.id = LayerId(2);
  shot.layers = {a, b};
  shot.links = {Link{{LayerId(1), {}}, {LayerId(2), {}}, Frame(0), 1.0},
                Link{{LayerId(2), "arm"}, {LayerId(3), {}}, Frame(0), 1.0}};
  QVERIFY(FindLink(shot, {LayerId(1), {}}) != nullptr);
  QVERIFY(FindLink(shot, {LayerId(1), "arm"}) == nullptr);
  DropDeadLinks(&shot);
  QCOMPARE(shot.links.size(), size_t{1});
  shot.layers.pop_back();
  DropDeadLinks(&shot);
  QVERIFY(shot.links.empty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ModelTests)
#include "model_tests.moc"
