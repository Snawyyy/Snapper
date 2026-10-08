#include <QLineF>
#include <QTest>

#include <cassert>
#include <cmath>
#include <memory>

#include "anim/doll_pose.h"
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

PiecePose At(double x, double y) {
  assert(std::isfinite(x));
  assert(std::isfinite(y));
  PiecePose pose;
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

// A body with an arm hanging from it.
Doll Body() {
  Doll doll;
  doll.name = "Bob";
  doll.art.pieces = {{"body", {"body.png"}, 0, {0, 0}, {20, 40}},
                     {"arm", {"arm.png"}, 0, {20, 0}, {30, 8}}};
  doll.rig.pieces = {{"body", "", {10, 20}, 0, -1, {}},
                     {"arm", "body", {0, 4}, 1, -1, {}}};
  assert(doll.art.pieces.size() == doll.rig.pieces.size());
  assert(FindRig(doll.rig, "arm") != nullptr);
  return doll;
}

// The box bounces up 100 pixels over ten frames; a picture sits at
// (300, 0); Bob stands turned, scaled and flipped, his body bent.
Project Stage() {
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(Body());
  Layer bob;
  bob.id = LayerId(3);
  DollLayer posed{"Bob", {}, true};
  PiecePose bent;
  bent.rotation = 30.0;
  SetKey(&posed.pieces["body"], {Frame(0), bent, Ease::kStep});
  bob.content = posed;
  PiecePose turned = At(-200, 50);
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
  assert(FindDoll(project, "Bob") != nullptr);
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

// Where the arm's joint is drawn at frame, links included.
QPointF ArmJoint(const Project& project, LinkSolver* links, int frame) {
  assert(links != nullptr);
  const Shot& shot = *project.shots[0];
  const Layer* bob = FindLayer(shot, LayerId(3));
  assert(bob != nullptr);
  const Doll& doll = *project.dolls.at("Bob");
  const auto placed =
      PieceTransforms(doll, links->Poses(doll, *bob, Frame(frame)));
  return links->LayerTransform(*bob, Frame(frame))
      .map(placed.at("arm").map(QPointF(0, 4)));
}

// How far the drag node on Bob's arm trails at frame, links included.
double ArmTrail(const Project& project, int frame) {
  assert(frame >= 0);
  const Shot& shot = *project.shots[0];
  const Layer* bob = FindLayer(shot, LayerId(3));
  assert(bob != nullptr);
  LinkSolver links(project, shot);
  const Doll& doll = *project.dolls.at("Bob");
  const PoseMap poses = links.Poses(doll, *bob, Frame(frame));
  const auto arm = poses.find("arm");
  const bool has_warp = arm != poses.end() && arm->second.warp.size() > 3;
  return has_warp ? QLineF(QPointF(), arm->second.warp[3]).length() : 0.0;
}

}  // namespace

class LinkTests final : public QObject {
  Q_OBJECT

 private slots:
  void UnlinkedThingsStayPut();
  void FollowerCopiesTheMoveWithoutJumping();
  void StrengthScalesTheMove();
  void PiecesFollowInShotSpace();
  void ChainsPassMovesOn();
  void LoopsAreFound();
  void DragNodesFeelLinks();
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
      Link{{LayerId(2), {}}, {LayerId(1), {}}, Frame(4), 1.0}};
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
      Link{{LayerId(2), {}}, {LayerId(1), {}}, Frame(0), 0.5}};
  LinkSolver links(project, *project.shots[0]);
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 0), {300, 0}));
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 10), {300, -50}));
}

void LinkTests::PiecesFollowInShotSpace() {
  Project project = Stage();
  LinkSolver still(project, *project.shots[0]);
  const QPointF rest = ArmJoint(project, &still, 10);
  // However Bob is turned, flipped and bent, his arm rises straight up
  // with the box.
  OnlyShot(&project).links = {
      Link{{LayerId(3), "arm"}, {LayerId(1), {}}, Frame(0), 1.0}};
  LinkSolver links(project, *project.shots[0]);
  QVERIFY(Near(ArmJoint(project, &links, 10), rest + QPointF(0, -100)));
  QVERIFY(Near(ArmJoint(project, &links, 0), rest));
}

void LinkTests::ChainsPassMovesOn() {
  Project project = Stage();
  // The picture follows the box; Bob's body follows the picture.
  OnlyShot(&project).links = {
      Link{{LayerId(2), {}}, {LayerId(1), {}}, Frame(0), 1.0},
      Link{{LayerId(3), "body"}, {LayerId(2), {}}, Frame(0), 1.0}};
  LinkSolver links(project, *project.shots[0]);
  Project unlinked = Stage();
  LinkSolver none(unlinked, *unlinked.shots[0]);
  QVERIFY(Near(OriginOf(&links, *project.shots[0], 2, 10), {300, -100}));
  // The arm hangs from the body, so it rises too.
  QVERIFY(Near(ArmJoint(project, &links, 10),
               ArmJoint(unlinked, &none, 10) + QPointF(0, -100)));
}

void LinkTests::LoopsAreFound() {
  Project project = Stage();
  OnlyShot(&project).links = {
      Link{{LayerId(2), {}}, {LayerId(1), {}}, Frame(0), 1.0}};
  const Shot& shot = *project.shots[0];
  QVERIFY(WouldLoop(project, shot, {LayerId(1), {}}, {LayerId(2), {}}));
  QVERIFY(!WouldLoop(project, shot, {LayerId(3), {}}, {LayerId(2), {}}));
  // A whole doll can't follow its own arm: the arm moves with it.
  QVERIFY(WouldLoop(project, shot, {LayerId(3), {}}, {LayerId(3), "arm"}));
  QVERIFY(WouldLoop(project, shot, {LayerId(3), "body"},
                    {LayerId(3), "arm"}));
  // Relinking the picture to Bob replaces its own link: no loop.
  QVERIFY(!WouldLoop(project, shot, {LayerId(2), {}}, {LayerId(3), {}}));
  const auto carriers = CarriersOf(project, shot, {LayerId(3), "arm"});
  QCOMPARE(carriers.size(), size_t{3});
  QCOMPARE(carriers[1], (LinkEnd{LayerId(3), "body"}));
  QCOMPARE(carriers[2], (LinkEnd{LayerId(3), {}}));
  // A loop that got into a file anyway stops instead of hanging.
  OnlyShot(&project).links.push_back(
      Link{{LayerId(1), {}}, {LayerId(2), {}}, Frame(0), 1.0});
  LinkSolver links(project, *project.shots[0]);
  const QPointF origin = OriginOf(&links, *project.shots[0], 2, 10);
  QVERIFY(std::isfinite(origin.x()) && std::isfinite(origin.y()));
}

void LinkTests::DragNodesFeelLinks() {
  Doll doll = Body();
  RigPiece* arm = nullptr;
  for (RigPiece& rig : doll.rig.pieces) {
    arm = rig.name == "arm" ? &rig : arm;
  }
  QVERIFY(arm != nullptr);
  arm->warp = {2, 2};
  arm->drag_nodes = {{3, 0.5, 0.5}};
  Project project = Stage();
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  // Bob stands still, so on his own his arm's node never trails.
  QVERIFY(ArmTrail(project, 5) < kClose);
  // Following the rising box, the node lags behind the move.
  OnlyShot(&project).links = {
      Link{{LayerId(3), {}}, {LayerId(1), {}}, Frame(0), 1.0}};
  QVERIFY(ArmTrail(project, 5) > 1.0);
  OnlyShot(&project).links = {
      Link{{LayerId(3), "arm"}, {LayerId(1), {}}, Frame(0), 1.0}};
  QVERIFY(ArmTrail(project, 5) > 1.0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::LinkTests)
#include "link_tests.moc"
