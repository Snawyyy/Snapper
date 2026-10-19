#include <QTest>

#include <cassert>
#include <cmath>
#include <memory>

#include "anim/links.h"
#include "model/project.h"

namespace snapper {
namespace {

constexpr double kClose = 1e-6;

bool Near(QPointF a, QPointF b) {
  assert(std::isfinite(a.x()) && std::isfinite(a.y()));
  assert(std::isfinite(b.x()) && std::isfinite(b.y()));
  return std::abs(a.x() - b.x()) < kClose && std::abs(a.y() - b.y()) < kClose;
}

LayerPose At(double x, double y) {
  assert(std::isfinite(x));
  assert(std::isfinite(y));
  LayerPose pose;
  pose.offset = QPointF(x, y);
  return pose;
}

// A layer whose own move goes linearly from `from` at frame 0 to `to` at
// frame 10.
Layer Moving(int id, QPointF from, QPointF to) {
  assert(id > 0);
  Layer layer;
  layer.id = LayerId(id);
  layer.content = ImageLayer{"/box.png"};
  SetKey(&layer.transform, {Frame(0), At(from.x(), from.y()), Ease::kLinear});
  SetKey(&layer.transform, {Frame(10), At(to.x(), to.y()), Ease::kStep});
  assert(layer.transform.keys.size() == 2);
  return layer;
}

// The box bounces up 100 pixels over ten frames; a picture sits at
// (300, 0); Bob, a character, stands turned and scaled.
Project Stage() {
  Project project;
  Layer bob;
  bob.id = LayerId(3);
  bob.content = CharacterLayer{"Bob", {}, true};
  LayerPose turned = At(-200, 50);
  turned.rotation = 90.0;
  turned.scale_x = 2.0;
  turned.scale_y = 2.0;
  SetKey(&bob.transform, {Frame(0), turned, Ease::kStep});
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {Moving(1, {0, 0}, {0, -100}),
                 Moving(2, {300, 0}, {300, 0}), bob};
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots[0]->layers.size() == 3);
  return project;
}

Shot& OnlyShot(Project* project) {
  assert(project != nullptr);
  assert(project->shots.size() == 1);
  auto shot = std::make_shared<Shot>(*project->shots[0]);
  project->shots[0] = shot;
  return *shot;
}

QPointF OriginOf(LinkSolver* links, const Shot& shot, int layer, int frame) {
  assert(links != nullptr);
  const Layer* found = FindLayer(shot, LayerId(layer));
  assert(found != nullptr);
  return links->LayerTransform(*found, Frame(frame)).map(QPointF());
}

}  // namespace

class LinkTests final : public QObject {
  Q_OBJECT

 private slots:
  void UnlinkedThingsStayPut();
  void FollowerCopiesTheMoveWithoutJumping();
  void StrengthScalesTheMove();
  void TurnedLayersFollowInShotSpace();
  void ChainsPassMovesOn();
  void LoopsAreFound();
};

void LinkTests::UnlinkedThingsStayPut() {
  const Project project = Stage();
  QVERIFY(project.shots[0]->links.empty());
  LinkSolver links(project, *project.shots[0]);
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 5), {300, 0}));
}

void LinkTests::FollowerCopiesTheMoveWithoutJumping() {
  Project project = Stage();
  OnlyShot(&project).links = {
      Link{{LayerId(2)}, {LayerId(1)}, Frame(4), 1.0}};
  const Shot& shot = *project.shots[0];
  LinkSolver links(project, shot);
  // At the link's frame it sits where its keys put it...
  QVERIFY(Near(OriginOf(&links, shot, 2, 4), {300, 0}));
  // ...then copies the box's rise from there, before and after.
  QVERIFY(Near(OriginOf(&links, shot, 2, 10), {300, -60}));
  QVERIFY(Near(OriginOf(&links, shot, 2, 0), {300, 40}));
  QVERIFY(Near(links.Push(shot.links[0], Frame(9)), {0, -50}));
}

void LinkTests::StrengthScalesTheMove() {
  Project project = Stage();
  OnlyShot(&project).links = {
      Link{{LayerId(2)}, {LayerId(1)}, Frame(0), 0.5}};
  LinkSolver links(project, *project.shots[0]);
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 0), {300, 0}));
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 10), {300, -50}));
}

void LinkTests::TurnedLayersFollowInShotSpace() {
  Project project = Stage();
  LinkSolver still(project, *project.shots[0]);
  const QPointF rest = OriginOf(&still, *project.shots[0], 3, 10);
  // However Bob is turned, scaled and flipped, he rises straight up with
  // the box.
  OnlyShot(&project).links = {
      Link{{LayerId(3)}, {LayerId(1)}, Frame(0), 1.0}};
  LinkSolver links(project, *project.shots[0]);
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 3, 10),
               rest + QPointF(0, -100)));
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 3, 0), rest));
}

void LinkTests::ChainsPassMovesOn() {
  Project project = Stage();
  // The picture follows the box; Bob follows the picture.
  OnlyShot(&project).links = {
      Link{{LayerId(2)}, {LayerId(1)}, Frame(0), 1.0},
      Link{{LayerId(3)}, {LayerId(2)}, Frame(0), 1.0}};
  LinkSolver links(project, *project.shots[0]);
  Project unlinked = Stage();
  LinkSolver none(unlinked, *unlinked.shots[0]);
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 10), {300, -100}));
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 3, 10),
               OriginOf(&none, *unlinked.shots[0], 3, 10) +
                   QPointF(0, -100)));
}

void LinkTests::LoopsAreFound() {
  Project project = Stage();
  OnlyShot(&project).links = {
      Link{{LayerId(2)}, {LayerId(1)}, Frame(0), 1.0}};
  const Shot& shot = *project.shots[0];
  QVERIFY(WouldLoop(project, shot, {LayerId(1)}, {LayerId(2)}));
  QVERIFY(!WouldLoop(project, shot, {LayerId(3)}, {LayerId(2)}));
  // Nothing follows itself.
  QVERIFY(WouldLoop(project, shot, {LayerId(3)}, {LayerId(3)}));
  // Relinking the picture to Bob replaces its own link: no loop.
  QVERIFY(!WouldLoop(project, shot, {LayerId(2)}, {LayerId(3)}));
  // A loop that got into a file anyway stops instead of hanging.
  OnlyShot(&project).links.push_back(
      Link{{LayerId(1)}, {LayerId(2)}, Frame(0), 1.0});
  LinkSolver links(project, *project.shots[0]);
  const QPointF origin = OriginOf(&links, *project.shots[0], 2, 10);
  QVERIFY(std::isfinite(origin.x()) && std::isfinite(origin.y()));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::LinkTests)
#include "link_tests.moc"
