#include <QDir>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>
#include <cmath>
#include <variant>

#include "anim/sampler.h"
#include "anim/warp.h"
#include "edit/history_manager.h"
#include "edit/mesh_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

constexpr QSize kSide(60, 40);

// A wedge pointing left (or right when mirrored), saved as name.
void SaveWedge(const QTemporaryDir& dir, const QString& name,
               bool is_mirrored, int width) {
  assert(dir.isValid());
  assert(width > 10);
  QImage image(kSide, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::blue);
  const QPolygonF wedge{QPointF(2, 20), QPointF(width, 4),
                        QPointF(width, 36)};
  painter.setTransform(is_mirrored
                           ? QTransform(-1, 0, 0, 1, kSide.width(), 0)
                           : QTransform());
  painter.drawPolygon(wedge);
  painter.end();
  QVERIFY(image.save(QDir(dir.path()).filePath(name)));
}

// Bob: a skirt on an old 2 by 2 grid with a drag node in the middle
// and a key that slides its warp, and two mirrored arms.
Project Bob(const QTemporaryDir& dir) {
  assert(dir.isValid());
  assert(!kSide.isEmpty());
  SaveWedge(dir, "skirt.png", false, 58);
  SaveWedge(dir, "arm_l.png", false, 50);
  SaveWedge(dir, "arm_r.png", true, 50);
  Doll doll;
  doll.name = "Bob";
  doll.folder = dir.path();
  doll.art.pieces = {{"skirt", {"skirt.png"}, 0, {-30, 0}, kSide},
                     {"arm_l", {"arm_l.png"}, 0, {-90, 0}, kSide},
                     {"arm_r", {"arm_r.png"}, 0, {30, 0}, kSide}};
  RigPiece skirt{"skirt", "", {30, 0}, 0, -1, WarpGrid{2, 2}};
  skirt.drag_nodes = {DragNode{4}};
  doll.rig.pieces = {skirt, {"arm_l", "", {50, 20}, 1, -1, {}},
                     {"arm_r", "", {10, 20}, 1, -1, {}}};
  Layer layer;
  layer.content = DollLayer{"Bob", {}, false};
  PiecePose slid;
  slid.warp.assign(9, QPointF(4, -2));
  SetKey(&std::get<DollLayer>(layer.content).pieces["skirt"],
         {Frame(0), slid, Ease::kStep});
  Shot shot;
  shot.layers.push_back(layer);
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots.push_back(std::make_shared<const Shot>(shot));
  return project;
}

const RigPiece& Piece(const HistoryManager& history, const QString& name) {
  assert(FindDoll(history.current(), "Bob") != nullptr);
  assert(!name.isEmpty());
  return *FindRig(FindDoll(history.current(), "Bob")->rig, name);
}

const std::vector<QPointF>& SkirtKey(const HistoryManager& history) {
  const Layer& layer = history.current().shots[0]->layers[0];
  assert(std::holds_alternative<DollLayer>(layer.content));
  assert(!history.current().shots.empty());
  return std::get<DollLayer>(layer.content)
      .pieces.at("skirt")
      .keys[0]
      .value.warp;
}

}  // namespace

class MeshEditTests final : public QObject {
  Q_OBJECT

 private slots:
  void ConvertCarriesDotsAndKeysInOneStep();
  void TheSameMeshAgainChangesNothing();
  void DoneTextSaysWhatCarried();
  void RedrawnArtIsMeshedAgain();
  void MirrorPairsMeshDotsByPlace();
  void GreyedReasonsFollowTheWarp();
  void CopyToTheOtherSideMeshesTheTwinsOwnDrawing();
};

void MeshEditTests::ConvertCarriesDotsAndKeysInOneStep() {
  QTemporaryDir dir;
  HistoryManager history(Bob(dir));
  RigManager rig(&history);
  const auto made = rig.SetShapeMeshAll("Bob", {"skirt"}, MeshDetail::kNormal);
  QVERIFY(made.has_value());
  QCOMPARE(made->carried, 1);
  const RigPiece skirt = Piece(history, "skirt");
  QVERIFY(skirt.warp.IsMesh());
  QCOMPARE(skirt.drag_nodes.size(), size_t{1});
  QCOMPARE(history.UndoLabel(), QString("Shape mesh of skirt"));
  const std::vector<QPointF>& key = SkirtKey(history);
  QCOMPARE(key.size(), static_cast<size_t>(skirt.warp.PointCount()));
  for (const QPointF& offset : key) {
    QVERIFY(std::hypot(offset.x() - 4, offset.y() + 2) < 1e-9);
  }
  history.Undo();
  QCOMPARE(Piece(history, "skirt").warp, (WarpGrid{2, 2}));
  QCOMPARE(SkirtKey(history).size(), size_t{9});
}

void MeshEditTests::TheSameMeshAgainChangesNothing() {
  QTemporaryDir dir;
  HistoryManager history(Bob(dir));
  RigManager rig(&history);
  QVERIFY(rig.SetShapeMeshAll("Bob", {"skirt"}, MeshDetail::kFew));
  const int revision = history.revision();
  const auto again = rig.SetShapeMeshAll("Bob", {"skirt"}, MeshDetail::kFew);
  QVERIFY(again.has_value());
  QCOMPARE(history.revision(), revision);
  QVERIFY(!rig.SetShapeMeshAll("Bob", {"nobody"}, MeshDetail::kFew));
}

void MeshEditTests::DoneTextSaysWhatCarried() {
  QCOMPARE(RigManager::MeshDoneText({}), QString("Shape mesh made."));
  QCOMPARE(RigManager::MeshDoneText({1, 0}),
           QString("Shape mesh made: 1 dot's settings carried over."));
  QVERIFY(RigManager::MeshDoneText({3, 2}).contains("2 found no dot"));
}

void MeshEditTests::RedrawnArtIsMeshedAgain() {
  QTemporaryDir dir;
  HistoryManager history(Bob(dir));
  RigManager rig(&history);
  QVERIFY(rig.SetShapeMeshAll("Bob", {"skirt"}, MeshDetail::kNormal));
  CarryCount count;
  QStringList remeshed;
  const Project same =
      WithFreshMeshes(history.current(), "Bob", &count, &remeshed);
  QVERIFY(remeshed.isEmpty());
  QVERIFY(same.dolls.at("Bob") == history.current().dolls.at("Bob"));
  SaveWedge(dir, "skirt.png", false, 30);
  const Project redrawn =
      WithFreshMeshes(history.current(), "Bob", &count, &remeshed);
  QCOMPARE(remeshed, QStringList{"skirt"});
  QCOMPARE(count.carried, 1);
  const RigPiece& skirt = *FindRig(redrawn.dolls.at("Bob")->rig, "skirt");
  QVERIFY(!(skirt.warp == Piece(history, "skirt").warp));
  for (const QPointF& point : RestPoints(kSide, skirt.warp)) {
    QVERIFY(point.x() < 34.0);
  }
}

void MeshEditTests::MirrorPairsMeshDotsByPlace() {
  QTemporaryDir dir;
  HistoryManager history(Bob(dir));
  RigManager rig(&history);
  QVERIFY(rig.SetShapeMeshAll("Bob", {"arm_l", "arm_r"},
                              MeshDetail::kNormal));
  const WarpGrid left = Piece(history, "arm_l").warp;
  const std::vector<QPointF> rest = RestPoints(kSide, left);
  const int tip = NearestPoint(rest, QPointF(2, 20), 1e9);
  QVERIFY(rig.SetDragNodes({{"Bob", "arm_l", tip}}, true));
  QCOMPARE(rig.WhyNoMirror("Bob", "arm_l"), QString());
  QVERIFY(rig.MirrorNodes("Bob", "arm_l"));
  const RigPiece right = Piece(history, "arm_r");
  QCOMPARE(right.drag_nodes.size(), size_t{1});
  const QPointF twin = RestPoints(kSide, right.warp)[static_cast<size_t>(
      right.drag_nodes[0].point)];
  QVERIFY2(std::hypot(twin.x() - 58, twin.y() - 20) < 6.0,
           qPrintable(QString("%1 %2").arg(twin.x()).arg(twin.y())));
}

void MeshEditTests::GreyedReasonsFollowTheWarp() {
  QTemporaryDir dir;
  HistoryManager history(Bob(dir));
  RigManager rig(&history);
  QVERIFY(rig.WhyNoDetail("Bob", "skirt").contains("old grid"));
  QCOMPARE(rig.WhyNoConvert("Bob", "skirt"), QString());
  QVERIFY(rig.WhyNoDetail("Bob", "arm_l").contains("Bend"));
  QVERIFY(!rig.WhyNoConvert("Bob", "arm_l").isEmpty());
  QVERIFY(!rig.WhyNoMirror("Bob", "arm_l").isEmpty());
  QVERIFY(rig.SetShapeMeshAll("Bob", {"skirt"}, MeshDetail::kNormal));
  QCOMPARE(rig.WhyNoDetail("Bob", "skirt"), QString());
  QVERIFY(!rig.WhyNoConvert("Bob", "skirt").isEmpty());
}

void MeshEditTests::CopyToTheOtherSideMeshesTheTwinsOwnDrawing() {
  QTemporaryDir dir;
  HistoryManager history(Bob(dir));
  RigManager rig(&history);
  QVERIFY(rig.SetShapeMeshAll("Bob", {"arm_l"}, MeshDetail::kFew));
  QVERIFY(rig.CopyToOtherSide("Bob", "arm_l", false));
  const RigPiece right = Piece(history, "arm_r");
  QVERIFY(right.warp.IsMesh());
  QCOMPARE(right.warp.mesh->detail, MeshDetail::kFew);
  // Its own drawing points the other way, so its dots do too.
  for (const QPointF& point : RestPoints(kSide, right.warp)) {
    QVERIFY(point.x() > 6.0);
  }
}

}  // namespace snapper

QTEST_MAIN(snapper::MeshEditTests)
#include "mesh_edit_tests.moc"
