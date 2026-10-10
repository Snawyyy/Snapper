#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

#include <cassert>

#include "io/doll_json.h"

namespace snapper {
namespace {

// A small two-triangle mesh over a 40 by 30 drawing.
WarpGrid Square() {
  const auto mesh = MakeWarpMesh(
      QSize(40, 30), MeshDetail::kMany,
      {{-1.5, -1.25}, {41.0, -1.0}, {41.5, 31.0}, {-1.0, 30.75}},
      {{0, 1, 2}, {0, 2, 3}});
  assert(mesh != nullptr);
  assert(mesh->points.size() == 4);
  return WarpGrid{0, 0, mesh};
}

Rig OnePiece(const WarpGrid& grid) {
  Rig rig;
  rig.pieces = {{"skirt", "", {20, 0}, 0, -1, grid}};
  rig.pieces[0].drag_nodes = {DragNode{2}};
  assert(rig.pieces.size() == 1);
  assert(rig.pieces[0].warp == grid);
  return rig;
}

}  // namespace

class WarpJsonTests : public QObject {
  Q_OBJECT

 private slots:
  void MeshRoundTripsExactly();
  void OldGridFilesLoadAsTheSameGrid();
  void BrokenMeshReadsAsNoWarp();
};

void WarpJsonTests::MeshRoundTripsExactly() {
  const Rig rig = OnePiece(Square());
  // Through text, as a file keeps it.
  const QByteArray text = QJsonDocument(RigToJson(rig)).toJson();
  JsonIssues issues;
  const Rig back = RigFromJson(QJsonDocument::fromJson(text).object(),
                               &issues);
  QVERIFY(!issues.HasIssue());
  QCOMPARE(back, rig);
  QVERIFY(back.pieces[0].warp.IsMesh());
  QCOMPARE(back.pieces[0].warp.mesh->detail, MeshDetail::kMany);
  QCOMPARE(back.pieces[0].drag_nodes.size(), size_t{1});
}

void WarpJsonTests::OldGridFilesLoadAsTheSameGrid() {
  const QJsonObject old = QJsonDocument::fromJson(R"({"pieces": [
      {"name": "skirt", "parent": "", "pivot": [20, 0], "order": 0,
       "warp": [4, 3], "drag": [{"point": 7}]}], "chains": []})")
                              .object();
  JsonIssues issues;
  const Rig rig = RigFromJson(old, &issues);
  QCOMPARE(rig.pieces[0].warp, (WarpGrid{4, 3}));
  QVERIFY(!rig.pieces[0].warp.IsMesh());
  QCOMPARE(rig.pieces[0].drag_nodes[0].point, 7);
  // Written back, an old grid has no mesh and the same cells.
  const QJsonObject piece = RigToJson(rig)["pieces"].toArray()[0].toObject();
  QVERIFY(!piece.contains("mesh"));
  QCOMPARE(piece["warp"].toArray(), (QJsonArray{4, 3}));
}

void WarpJsonTests::BrokenMeshReadsAsNoWarp() {
  QJsonObject piece = RigToJson(OnePiece(Square()))["pieces"]
                          .toArray()[0]
                          .toObject();
  QJsonObject mesh = piece["mesh"].toObject();
  mesh["triangles"] = QJsonArray{0, 1, 9};
  piece["mesh"] = mesh;
  JsonIssues issues;
  const Rig rig = RigFromJson(
      QJsonObject{{"pieces", QJsonArray{piece}}, {"chains", QJsonArray{}}},
      &issues);
  QVERIFY(issues.HasIssue());
  QVERIFY(!rig.pieces[0].warp.IsOn());
  QVERIFY(rig.pieces[0].drag_nodes.empty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::WarpJsonTests)
#include "warp_json_tests.moc"
