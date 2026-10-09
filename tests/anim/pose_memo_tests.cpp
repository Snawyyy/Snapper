#include <QTest>

#include <cassert>
#include <memory>

#include "anim/links.h"
#include "anim/pose_memo.h"

namespace snapper {
namespace {

constexpr LayerId kLayer(1);

Project OneShotProject() {
  Layer layer;
  layer.id = kLayer;
  layer.content = DollLayer{"Dot", {}, false};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.dolls["Dot"] = std::make_shared<const Doll>();
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots.size() == 1);
  assert(project.dolls.size() == 1);
  return project;
}

PoseMap OnePose(double degrees) {
  PoseMap poses;
  assert(degrees >= 0.0);
  poses["body"].rotation = degrees;
  assert(poses.size() == 1);
  return poses;
}

}  // namespace

class PoseMemoTests final : public QObject {
  Q_OBJECT

 private slots:
  void KeepsAPoseForItsLayerAndFrame();
  void AnEditedShotEmptiesIt();
  void AShotOutsideTheProjectKeepsNothing();
  void ASolverReusesWhatAnotherSolved();
  void IsBoundedInSize();
};

void PoseMemoTests::KeepsAPoseForItsLayerAndFrame() {
  const Project project = OneShotProject();
  PoseMemo memo;
  QVERIFY(memo.Bind(project, *project.shots[0]));
  QVERIFY(memo.Find(kLayer, Frame(3)) == nullptr);
  memo.Store(kLayer, Frame(3), OnePose(30.0));
  const PoseMap* kept = memo.Find(kLayer, Frame(3));
  QVERIFY(kept != nullptr);
  QCOMPARE(kept->at("body").rotation, 30.0);
  QVERIFY(memo.Find(kLayer, Frame(4)) == nullptr);
  QVERIFY(memo.Bind(project, *project.shots[0]));
  QVERIFY(memo.Find(kLayer, Frame(3)) != nullptr);
}

void PoseMemoTests::AnEditedShotEmptiesIt() {
  Project project = OneShotProject();
  PoseMemo memo;
  QVERIFY(memo.Bind(project, *project.shots[0]));
  memo.Store(kLayer, Frame(0), OnePose(5.0));
  Shot edited = *project.shots[0];
  edited.name = "edited";
  project.shots[0] = std::make_shared<const Shot>(edited);
  QVERIFY(memo.Bind(project, *project.shots[0]));
  QCOMPARE(memo.size(), size_t{0});
  memo.Store(kLayer, Frame(0), OnePose(5.0));
  project.dolls["Dot"] = std::make_shared<const Doll>();
  QVERIFY(memo.Bind(project, *project.shots[0]));
  QCOMPARE(memo.size(), size_t{0});
}

void PoseMemoTests::AShotOutsideTheProjectKeepsNothing() {
  const Project project = OneShotProject();
  const Shot loose = *project.shots[0];
  PoseMemo memo;
  QVERIFY(!memo.Bind(project, loose));
  QCOMPARE(memo.size(), size_t{0});
}

void PoseMemoTests::ASolverReusesWhatAnotherSolved() {
  const Project project = OneShotProject();
  const Shot& shot = *project.shots[0];
  const Doll& doll = *project.dolls.at("Dot");
  PoseMemo memo;
  LinkSolver first(project, shot, &memo);
  first.Poses(doll, shot.layers[0], Frame(2));
  QCOMPARE(memo.size(), size_t{1});
  memo.Store(kLayer, Frame(2), OnePose(77.0));
  LinkSolver second(project, shot, &memo);
  const PoseMap again = second.Poses(doll, shot.layers[0], Frame(2));
  QCOMPARE(again.at("body").rotation, 77.0);
}

void PoseMemoTests::IsBoundedInSize() {
  const Project project = OneShotProject();
  PoseMemo memo;
  QVERIFY(memo.Bind(project, *project.shots[0]));
  for (int i = 0; i < static_cast<int>(kMaxMemoPoses) + 10; ++i) {
    memo.Store(kLayer, Frame(i), OnePose(1.0));
  }
  QVERIFY(memo.size() <= kMaxMemoPoses);
  QVERIFY(memo.size() > 0);
}

}  // namespace snapper

QTEST_MAIN(snapper::PoseMemoTests)
#include "pose_memo_tests.moc"
